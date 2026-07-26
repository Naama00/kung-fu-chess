// engine/core/MatchController.cpp
#include "engine/core/MatchController.hpp"
#include "engine/common/BoardPresets.hpp"
#include "engine/common/PieceTokenCodec.hpp"
#include "engine/common/PieceValues.hpp"
#include "engine/io/BoardParser.hpp"
#include "engine/snapshot/SnapshotBuilder.hpp"
#include "players/ai/ClassicMinimaxStrategy.hpp"
#include "players/ai/GenericAIPlayer.hpp"
#include "players/ai/RealTimeStrategies.hpp"
#include "players/human/InputConfig.hpp"
#include "players/network/ClientAuth.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>

namespace kungfu {

MatchController::MatchController(const GameConfig& config,
                                 bool isSimultaneous,
                                 bool isAiOpponent,
                                 AiDifficulty aiDifficulty,
                                 bool isNetworkMode,
                                 std::string host,
                                 std::string port,
                                 bool isSpectator,
                                 std::uint64_t spectateMatchId,
                                 std::uint64_t onlineRoomCode,
                                 std::shared_ptr<NetworkSession> authSession)
    : m_config(config),
      m_isSimultaneous(isSimultaneous),
      m_isAiOpponent(isAiOpponent),
      m_aiDifficulty(aiDifficulty),
      m_isNetworkMode(isNetworkMode),
      m_host(std::move(host)),
      m_port(std::move(port)),
      m_isSpectator(isSpectator),
      m_spectateMatchId(spectateMatchId),
      m_onlineRoomCode(onlineRoomCode),
      m_authSession(std::move(authSession)) {

    if (!m_isSimultaneous) {
        m_config.cooldownDurationMs = 0;
        m_config.allowJumping = false;
        m_config.enablePremoves = false;
    }

    initializeMatch();

    if (m_isNetworkMode) {
        m_isAiOpponent = false;
        m_aiPlayer = nullptr;

        if (m_authSession) {
            m_networkPlayer = m_authSession->player;
            if (m_networkPlayer) {
                m_networkPlayer->resetMatchState();
                m_networkPlayer->beginPlay(false, 0, m_onlineRoomCode);
            }
        } else {
            m_networkPlayer = std::make_shared<NetworkPlayer>(
                m_ioContext, m_host, m_port, m_isSpectator, m_spectateMatchId, m_onlineRoomCode);
            m_networkPlayer->connectAndJoin();

            m_networkThread = std::thread([this]() {
                boost::asio::io_context::work work(m_ioContext);
                m_ioContext.run();
            });
        }
    }
}

MatchController::~MatchController() {
    if (m_isNetworkMode) {
        if (m_networkPlayer) {
            m_networkPlayer->handleDisconnect();
        }
        if (!m_authSession) {
            m_ioContext.stop();
            if (m_networkThread.joinable()) {
                m_networkThread.join();
            }
        }
        m_networkPlayer.reset();
    }

    if (m_aiFuture.valid()) {
        m_aiFuture.wait();
    }
}

void MatchController::initializeMatch() {
    if (m_aiFuture.valid()) {
        m_aiFuture.wait();
    }

    m_eventBus = std::make_shared<EventBus>();
    auto board = BoardParser::parse(BoardPresets::kStandardStartBoard);

    m_gameEngine = std::make_shared<GameEngine>(
        board,
        std::make_shared<RuleEngine>(board),
        m_config,
        std::make_shared<ChessPromotionRule>(),
        m_eventBus);

    m_humanPlayer = std::make_shared<HumanPlayer>(m_gameEngine);
    m_aiPlayer = m_isAiOpponent
                     ? std::make_shared<GenericAIPlayer>(PlayerColor::Black, createAiStrategy())
                     : nullptr;
    m_humanPlayer->setCellSize(InputConfig::kDefaultCellSize);

    m_eventBus->subscribe<MoveCompletedEvent>([this](const MoveCompletedEvent& ev) {
        if (ev.detail.cancelled || !ev.detail.piece) return;

        std::string notation = getMoveNotationString(
            ev.detail.piece->type(),
            ev.detail.from,
            ev.detail.to);
        addHistoryLog(ev.detail.piece->color(), notation);
    });

    m_isPaused = false;
    m_aiThinking = false;
    m_aiActionPending = false;
    m_wasMatchStarted = false;
    m_whiteHistory = {"Connected"};
    m_blackHistory = {"Connected"};

    m_eventBus->publish(GameTransitionEvent{GameTransitionType::Started, PlayerColor::White});
}

void MatchController::resetMatch() {
    if (m_isNetworkMode && m_networkPlayer) {
        m_networkPlayer->resetMatchState();
        if (m_authSession) {
            m_networkPlayer->beginPlay(false, 0, m_networkPlayer->onlineRoomCode());
        } else {
            m_networkPlayer->connectAndJoin();
        }
    }
    initializeMatch();
}

void MatchController::cancelMatchmaking() {
    if (m_networkPlayer) {
        m_networkPlayer->handleDisconnect();
    }
}

void MatchController::togglePause() {
    if (m_isNetworkMode) return;
    m_isPaused = !m_isPaused;
}

std::unique_ptr<IAIDecisionStrategy> MatchController::createAiStrategy() const {
    if (m_config.allowSimultaneousMovement) {
        if (m_aiDifficulty == AiDifficulty::Easy) return std::make_unique<RealTimeEasyStrategy>();
        if (m_aiDifficulty == AiDifficulty::Medium) return std::make_unique<RealTimeMediumStrategy>();
        return std::make_unique<RealTimeHardStrategy>();
    }
    int depth = (m_aiDifficulty == AiDifficulty::Easy) ? 1 : (m_aiDifficulty == AiDifficulty::Medium) ? 2 : 3;
    return std::make_unique<ClassicMinimaxStrategy>(depth);
}

void MatchController::addHistoryLog(PlayerColor color, const std::string& logText) {
    auto& history = (color == PlayerColor::White) ? m_whiteHistory : m_blackHistory;
    history.push_back(logText);
    if (history.size() > 8) {
        history.erase(history.begin());
    }
}

std::string MatchController::getMoveNotationString(PieceType type, const Position& from, const Position& to) const {
    char pieceChar = PieceTokenCodec::toChar(type);
    std::string notation = {pieceChar, ':', ' ', static_cast<char>('a' + to.col()), static_cast<char>('1' + to.row())};
    if (from == to) {
        notation += " (Jump)";
    }
    return notation;
}

PieceType MatchController::getPieceTypeAt(const Position& pos) const {
    if (!m_gameEngine) return PieceType::Pawn;
    if (auto p = m_gameEngine->getBoard()->pieceAt(pos)) {
        return p.value()->type();
    }
    if (auto p = m_gameEngine->getArbiter().getPieceInTransitAt(pos)) {
        return p.value()->type();
    }
    return PieceType::Pawn;
}

void MatchController::applySyncedBoard(const std::string& boardText) {
    auto board = BoardParser::parse(boardText);
    auto ruleEngine = std::make_shared<RuleEngine>(board);

    m_gameEngine = std::make_shared<GameEngine>(board, ruleEngine, m_config, std::make_shared<ChessPromotionRule>(), m_eventBus);
    m_humanPlayer = std::make_shared<HumanPlayer>(m_gameEngine);
    m_humanPlayer->setCellSize(InputConfig::kDefaultCellSize);

    std::cout << "[Spectator] Board successfully synchronized with live room!" << std::endl;
}

bool MatchController::handleTileClick(int row, int col) {
    if (m_isPaused || isGameOver()) return false;

    if (m_isNetworkMode && m_networkPlayer) {
        if (!m_networkPlayer->hasMatchStarted() || m_networkPlayer->isOpponentDisconnectedWithCountdown()) {
            return false;
        }

        auto clickedColorOpt = m_gameEngine->getPieceColorAt({row, col});
        if (!m_humanPlayer->selectedPosition().has_value() && clickedColorOpt.has_value()) {
            if (clickedColorOpt.value() != m_networkPlayer->assignedColor()) {
                return false;
            }
        }
    } else if (m_isAiOpponent && !m_humanPlayer->selectedPosition().has_value()) {
        if (m_gameEngine->getPieceColorAt({row, col}) == PlayerColor::Black) {
            return false;
        }
    }

    auto selectedBefore = m_humanPlayer->selectedPosition();

    if (m_isNetworkMode) {
        if (selectedBefore.has_value()) {
            Position from = selectedBefore.value();
            Position to(row, col);

            if (from == to) {
                m_humanPlayer->clearSelection();
            } else {
                auto boardNonConst = std::const_pointer_cast<IBoard>(m_gameEngine->getBoard());
                RuleEngine validator(boardNonConst);
                if (validator.validateMove(from, to).isValid) {
                    m_networkPlayer->sendMoveToServer(PlayerAction(from, to));
                }
                m_humanPlayer->clearSelection();
            }
        } else {
            const int cellSize = InputConfig::kDefaultCellSize;
            const int halfCell = cellSize / 2;
            m_humanPlayer->handleClick(col * cellSize + halfCell, row * cellSize + halfCell);
        }
    } else {
        m_humanPlayer->handleClick(col * 100 + 50, row * 100 + 50);
    }
    return true;
}

void MatchController::handleJump(const Position& pos) {
    if (m_isNetworkMode && m_networkPlayer) {
        m_networkPlayer->sendMoveToServer(PlayerAction(pos, pos));
    } else {
        m_gameEngine->requestMove(pos, pos);
    }
    m_humanPlayer->clearSelection();
}

void MatchController::processAiTurn(float deltaTime) {
    if (m_isPaused || m_gameEngine->isGameOver() || !m_isAiOpponent || !m_aiPlayer) {
        return;
    }
    if (!m_config.allowSimultaneousMovement && m_gameEngine->currentTurn() == PlayerColor::White) {
        m_aiActionPending = false;
        return;
    }

    bool shouldAiMove = false;
    if (!m_config.allowSimultaneousMovement) {
        if (m_gameEngine->currentTurn() == PlayerColor::Black && !m_aiActionPending && !m_gameEngine->getArbiter().hasActiveMotion()) {
            shouldAiMove = true;
        }
    } else {
        m_aiDecisionTimer -= deltaTime;
        if (m_aiDecisionTimer <= 0.0f) {
            shouldAiMove = true;
            m_aiDecisionTimer = 1.0f + (static_cast<float>(std::rand()) / RAND_MAX) * 1.2f;
        }
    }

    if (shouldAiMove && !m_aiThinking) {
        m_aiThinking = true;
        auto snapshot = view::SnapshotBuilder::build(*m_gameEngine->getBoard(), m_gameEngine->getArbiter(), m_gameEngine->getCurrentTimeMs(), m_gameEngine->isGameOver(), std::nullopt);
        m_aiFuture = std::async(std::launch::async, [this, snapshot]() { return m_aiPlayer->decideActions(snapshot); });
    }

    if (m_aiThinking && m_aiFuture.valid()) {
        if (m_aiFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            auto aiRequests = m_aiFuture.get();
            m_aiThinking = m_aiActionPending = false;

            if (!aiRequests.empty()) {
                auto aiResults = m_gameEngine->processActionRequests(aiRequests);
                if (!aiResults.empty() && aiResults.front().status == ActionStatus::Accepted) {
                    if (!m_config.allowSimultaneousMovement) {
                        m_gameEngine->wait(1000);
                    }
                }
            }
        }
    }
}

void MatchController::update(float deltaTime) {
    if (m_isNetworkMode && m_networkPlayer && !m_networkPlayer->isSpectator() && !m_networkPlayer->hasMatchStarted()) {
        m_searchTimer += deltaTime;
    }

    if (m_isPaused) return;

    m_gameEngine->wait(static_cast<int>(deltaTime * 1000.0f));

    if (m_isNetworkMode && m_networkPlayer) {
        if (!m_wasMatchStarted && m_networkPlayer->hasMatchStarted()) {
            m_wasMatchStarted = true;
            if (m_eventBus) {
                m_eventBus->publish(PlaySoundEvent{"move"});
            }
        }

        if (m_networkPlayer->hasPendingSync()) {
            std::string syncedBoard = m_networkPlayer->consumePendingSync();
            applySyncedBoard(syncedBoard);
        }

        auto results = m_networkPlayer->pollResults();
        for (const auto& res : results) {
            if (res.status == ActionStatus::Rejected) {
                addHistoryLog(m_networkPlayer->assignedColor(), "Move rejected by server");
            }
        }

        bool gameOver = isGameOver();
        auto snapshot = view::SnapshotBuilder::build(*m_gameEngine->getBoard(), m_gameEngine->getArbiter(), m_gameEngine->getCurrentTimeMs(), gameOver, std::nullopt);
        auto networkActions = m_networkPlayer->decideActions(snapshot);
        for (const auto& req : networkActions) {
            m_gameEngine->applyServerMove(req.action.from, req.action.to);
        }
    } else {
        processAiTurn(deltaTime);
    }
}

view::GameSnapshot MatchController::getSnapshot() const {
    bool gameOver = isGameOver();
    auto selected = m_humanPlayer ? m_humanPlayer->selectedPosition() : std::nullopt;
    return view::SnapshotBuilder::build(*m_gameEngine->getBoard(), m_gameEngine->getArbiter(), m_gameEngine->getCurrentTimeMs(), gameOver, selected);
}

const PremoveQueue& MatchController::getPremoveQueue() const {
    return m_gameEngine->getPremoveQueue();
}

int MatchController::getCurrentTimeMs() const {
    return m_gameEngine ? m_gameEngine->getCurrentTimeMs() : 0;
}

bool MatchController::isPaused() const { return m_isPaused; }

bool MatchController::isGameOver() const {
    if (!m_gameEngine) return false;
    if (m_gameEngine->isGameOver()) return true;
    if (m_isNetworkMode && m_networkPlayer) {
        if (m_networkPlayer->matchEnded() || m_networkPlayer->opponentDisconnected()) {
            return true;
        }
    }
    return false;
}

bool MatchController::isNetworkMode() const { return m_isNetworkMode; }
bool MatchController::isAiOpponent() const { return m_isAiOpponent; }

bool MatchController::isSpectator() const {
    return m_isNetworkMode && m_networkPlayer && m_networkPlayer->isSpectator();
}

bool MatchController::hasMatchStarted() const {
    return m_isNetworkMode && m_networkPlayer && m_networkPlayer->hasMatchStarted();
}

bool MatchController::isOpponentDisconnectedWithCountdown() const {
    return m_isNetworkMode && m_networkPlayer && m_networkPlayer->isOpponentDisconnectedWithCountdown();
}

int MatchController::opponentDisconnectCountdown() const {
    return (m_isNetworkMode && m_networkPlayer) ? m_networkPlayer->opponentDisconnectCountdown() : 0;
}

bool MatchController::matchEnded() const {
    return m_isNetworkMode && m_networkPlayer && m_networkPlayer->matchEnded();
}

bool MatchController::opponentDisconnected() const {
    return m_isNetworkMode && m_networkPlayer && m_networkPlayer->opponentDisconnected();
}

float MatchController::searchTimer() const { return m_searchTimer; }

std::uint64_t MatchController::onlineRoomCode() const {
    return (m_isNetworkMode && m_networkPlayer) ? m_networkPlayer->onlineRoomCode() : m_onlineRoomCode;
}

bool MatchController::hasActiveMotion() const {
    return m_gameEngine && m_gameEngine->getArbiter().hasActiveMotion();
}

std::shared_ptr<EventBus> MatchController::getEventBus() const {
    return m_eventBus;
}

std::string MatchController::getPlayerName(PlayerColor color) const {
    if (m_isNetworkMode && m_networkPlayer) {
        if (m_networkPlayer->isSpectator()) {
            return (color == PlayerColor::White) ? "White Player" : "Black Player";
        }
        if (m_networkPlayer->assignedColor() == color) {
            return ClientAuth::username.empty() ? "You" : ClientAuth::username;
        } else {
            return m_networkPlayer->opponentUsername();
        }
    } else if (m_isAiOpponent) {
        if (color == PlayerColor::White) {
            return ClientAuth::username.empty() ? "Player 1" : ClientAuth::username;
        } else {
            std::string diffStr = (m_aiDifficulty == AiDifficulty::Easy) ? "Easy" : 
                                  (m_aiDifficulty == AiDifficulty::Medium) ? "Medium" : "Hard";
            return "AI (" + diffStr + ")";
        }
    } else {
        if (color == PlayerColor::White) {
            return ClientAuth::username.empty() ? "Player 1" : ClientAuth::username;
        } else {
            return "Player 2";
        }
    }
}

const std::vector<std::string>& MatchController::getHistory(PlayerColor color) const {
    return (color == PlayerColor::White) ? m_whiteHistory : m_blackHistory;
}

std::optional<PlayerColor> MatchController::determineWinnerColor() const {
    if (!m_gameEngine) return std::nullopt;
    auto board = m_gameEngine->getBoard();
    if (!board) return std::nullopt;

    bool hasWhiteKing = false;
    bool hasBlackKing = false;
    for (const auto& piece : board->pieces()) {
        if (piece && piece->type() == PieceType::King && piece->state() != PieceState::Captured) {
            if (piece->color() == PlayerColor::White) hasWhiteKing = true;
            if (piece->color() == PlayerColor::Black) hasBlackKing = true;
        }
    }
    if (hasWhiteKing && !hasBlackKing) return PlayerColor::White;
    if (!hasWhiteKing && hasBlackKing) return PlayerColor::Black;
    return std::nullopt;
}

std::optional<Position> MatchController::selectedPosition() const {
    return m_humanPlayer ? m_humanPlayer->selectedPosition() : std::nullopt;
}

void MatchController::clearSelection() {
    if (m_humanPlayer) {
        m_humanPlayer->clearSelection();
    }
}

} // namespace kungfu