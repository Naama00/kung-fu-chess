// ui/screens/ChessGameScreen.cpp
#include "ui/screens/ChessGameScreen.hpp"
#include "engine/snapshot/SnapshotBuilder.hpp"
#include "engine/io/BoardParser.hpp"
#include "engine/common/PieceTokenCodec.hpp"
#include "engine/common/PieceValues.hpp"
#include "engine/common/BoardPresets.hpp"
#include "engine/events/GameEvents.hpp"
#include "engine/rules/RuleEngine.hpp"  
#include "engine/rules/PromotionRules.hpp"     
#include "players/human/InputConfig.hpp"     
#include "players/ai/GenericAIPlayer.hpp"
#include "players/ai/ClassicMinimaxStrategy.hpp"
#include "players/ai/RealTimeStrategies.hpp"
#include "players/network/ClientAuth.hpp"
#include <iostream>
#include <cmath>
#include <algorithm>

ChessGameScreen::ChessGameScreen(ScreenManager &manager,
                                 bool isSimultaneousMode,
                                 bool isAiOpponent,
                                 AiDifficulty aiDifficulty,
                                 std::shared_ptr<ISoundPlayer> soundPlayer,
                                 bool isNetworkMode,
                                 std::string host,
                                 std::string port,
                                 bool isSpectator,
                                 std::uint64_t spectateMatchId,
                                 std::uint64_t onlineRoomCode)
    : BaseScreen(manager, "Chess Match"), m_isAiOpponent(isAiOpponent), m_aiDifficulty(aiDifficulty),
      m_soundPlayer(std::move(soundPlayer)), m_isNetworkMode(isNetworkMode)
{
    m_config.allowSimultaneousMovement = isSimultaneousMode;
    if (!m_config.allowSimultaneousMovement) {
        m_config.cooldownDurationMs = 0;
        m_config.allowJumping = m_config.enablePremoves = false;
    }
    initializeScreen();

    if (m_isNetworkMode) {
        m_isAiOpponent = false;
        m_aiPlayer = nullptr;

        m_networkPlayer = std::make_shared<kungfu::NetworkPlayer>(m_ioContext, host, port, isSpectator, spectateMatchId, onlineRoomCode);
        m_networkPlayer->connectAndJoin();

        m_networkThread = std::thread([this]() {
            boost::asio::io_context::work work(m_ioContext);
            m_ioContext.run();
        });
    }
}

ChessGameScreen::ChessGameScreen(ScreenManager &manager,
                                 bool isSimultaneousMode,
                                 std::shared_ptr<ISoundPlayer> soundPlayer,
                                 std::shared_ptr<kungfu::NetworkSession> authSession,
                                 std::uint64_t onlineRoomCode)
    : BaseScreen(manager, "Chess Match"), m_soundPlayer(std::move(soundPlayer)),
      m_isNetworkMode(true), m_authSession(authSession)
{
    m_config.allowSimultaneousMovement = isSimultaneousMode;
    if (!m_config.allowSimultaneousMovement) {
        m_config.cooldownDurationMs = 0;
        m_config.allowJumping = m_config.enablePremoves = false;
    }
    initializeScreen();

    m_isAiOpponent = false;
    m_aiPlayer = nullptr;
    m_networkPlayer = authSession->player;
    if (m_networkPlayer) {
        m_networkPlayer->resetMatchState();
        m_networkPlayer->beginPlay(false, 0, onlineRoomCode);
    }
}

ChessGameScreen::ChessGameScreen(ScreenManager &manager, bool isSimultaneousMode, bool isAiOpponent, std::shared_ptr<ISoundPlayer> soundPlayer)
    : ChessGameScreen(manager, isSimultaneousMode, isAiOpponent, AiDifficulty::Medium, soundPlayer, false, "127.0.0.1", "8080") {}

ChessGameScreen::~ChessGameScreen() {
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

std::string ChessGameScreen::getPlayerName(kungfu::PlayerColor color) const {
    if (m_isNetworkMode && m_networkPlayer) {
        if (m_networkPlayer->isSpectator()) {
            return (color == kungfu::PlayerColor::White) ? "White Player" : "Black Player";
        }
        if (m_networkPlayer->assignedColor() == color) {
            return kungfu::ClientAuth::username.empty() ? "You" : kungfu::ClientAuth::username;
        } else {
            return m_networkPlayer->opponentUsername();
        }
    } else if (m_isAiOpponent) {
        if (color == kungfu::PlayerColor::White) {
            return kungfu::ClientAuth::username.empty() ? "Player 1" : kungfu::ClientAuth::username;
        } else {
            std::string diffStr = (m_aiDifficulty == AiDifficulty::Easy) ? "Easy" : 
                                  (m_aiDifficulty == AiDifficulty::Medium) ? "Medium" : "Hard";
            return "AI (" + diffStr + ")";
        }
    } else {
        if (color == kungfu::PlayerColor::White) {
            return kungfu::ClientAuth::username.empty() ? "Player 1" : kungfu::ClientAuth::username;
        } else {
            return "Player 2";
        }
    }
}

kungfu::PieceType ChessGameScreen::getPieceTypeAt(const kungfu::Position &pos) const {
    if (auto p = m_gameEngine->getBoard()->pieceAt(pos))
        return p.value()->type();
    if (auto p = m_gameEngine->getArbiter().getPieceInTransitAt(pos))
        return p.value()->type();
    return kungfu::PieceType::Pawn;
}

int ChessGameScreen::calculateAbsoluteMaterialScore(const kungfu::view::GameSnapshot &snapshot, kungfu::PlayerColor color) const {
    int total = 0;
    for (const auto &piece : snapshot.pieces) {
        if (piece.color == color && piece.state != kungfu::PieceState::Captured) {
            total += kungfu::PieceValues::getStandardValue(piece.type);
        }
    }
    return total;
}

std::optional<kungfu::PlayerColor> ChessGameScreen::determineWinnerColor(const kungfu::view::GameSnapshot &snapshot) const {
    bool hasWhiteKing = false;
    bool hasBlackKing = false;
    for (const auto &piece : snapshot.pieces) {
        if (piece.type == kungfu::PieceType::King && piece.state != kungfu::PieceState::Captured) {
            if (piece.color == kungfu::PlayerColor::White) hasWhiteKing = true;
            if (piece.color == kungfu::PlayerColor::Black) hasBlackKing = true;
        }
    }
    if (hasWhiteKing && !hasBlackKing) return kungfu::PlayerColor::White;
    if (!hasWhiteKing && hasBlackKing) return kungfu::PlayerColor::Black;
    return std::nullopt;
}

std::string ChessGameScreen::getMoveNotationString(kungfu::PieceType type, const BoardPos &from, const BoardPos &to) const {
    char pieceChar = kungfu::PieceTokenCodec::toChar(type);
    std::string notation = {pieceChar, ':', ' ', static_cast<char>('a' + to.col), static_cast<char>('1' + to.row)};
    if (from == to) notation += " (Jump)";
    return notation;
}

void ChessGameScreen::addHistoryLog(kungfu::PlayerColor color, const std::string &logText) {
    auto &history = (color == kungfu::PlayerColor::White) ? m_whiteHistory : m_blackHistory;
    history.push_back(logText);
    if (history.size() > 8) history.erase(history.begin());
}

std::unique_ptr<kungfu::IAIDecisionStrategy> ChessGameScreen::createAiStrategy() const {
    if (m_config.allowSimultaneousMovement) {
        if (m_aiDifficulty == AiDifficulty::Easy) return std::make_unique<kungfu::RealTimeEasyStrategy>();
        if (m_aiDifficulty == AiDifficulty::Medium) return std::make_unique<kungfu::RealTimeMediumStrategy>();
        return std::make_unique<kungfu::RealTimeHardStrategy>();
    }
    int depth = (m_aiDifficulty == AiDifficulty::Easy) ? 1 : (m_aiDifficulty == AiDifficulty::Medium) ? 2 : 3;
    return std::make_unique<kungfu::ClassicMinimaxStrategy>(depth);
}

void ChessGameScreen::applySyncedBoard(const std::string &boardText) {
    auto board = kungfu::BoardParser::parse(boardText);
    auto ruleEngine = std::make_shared<kungfu::RuleEngine>(board);

    m_gameEngine = std::make_shared<kungfu::GameEngine>(board, ruleEngine, m_config, std::make_shared<kungfu::ChessPromotionRule>(), m_eventBus);
    m_humanPlayer = std::make_shared<kungfu::HumanPlayer>(m_gameEngine);
    m_humanPlayer->setCellSize(kungfu::InputConfig::kDefaultCellSize);

    std::cout << "[Spectator] Board successfully synchronized with live room!" << std::endl;
}

void ChessGameScreen::resetGame() {
    if (m_aiFuture.valid()) {
        m_aiFuture.wait();
    }

    m_eventBus = std::make_shared<kungfu::EventBus>();
    auto board = kungfu::BoardParser::parse(kungfu::BoardPresets::kStandardStartBoard);

    m_gameEngine = std::make_shared<kungfu::GameEngine>(
        board,
        std::make_shared<kungfu::RuleEngine>(board),
        m_config,
        std::make_shared<kungfu::ChessPromotionRule>(),
        m_eventBus);

    m_humanPlayer = std::make_shared<kungfu::HumanPlayer>(m_gameEngine);
    m_aiPlayer = m_isAiOpponent ? std::make_shared<kungfu::GenericAIPlayer>(kungfu::PlayerColor::Black, createAiStrategy()) : nullptr;
    m_humanPlayer->setCellSize(kungfu::InputConfig::kDefaultCellSize);

    m_eventBus->subscribe<kungfu::MoveCompletedEvent>([this](const kungfu::MoveCompletedEvent &ev) {
        if (ev.detail.cancelled || !ev.detail.piece) return;
        
        std::string notation = getMoveNotationString(
            ev.detail.piece->type(), 
            {ev.detail.from.row(), ev.detail.from.col()}, 
            {ev.detail.to.row(), ev.detail.to.col()}
        );
        addHistoryLog(ev.detail.piece->color(), notation);

        if (ev.detail.isCapture) {
            spawnCaptureExplosion(ev.detail.to, ev.detail.piece->color());
        }
    });

    m_eventBus->subscribe<kungfu::PlaySoundEvent>([this](const kungfu::PlaySoundEvent &ev) {
        if (m_soundPlayer) {
            m_soundPlayer->playSound(ev.soundId);
        }
    });

    m_eventBus->subscribe<kungfu::GameTransitionEvent>([this](const kungfu::GameTransitionEvent &ev) {
        if (ev.type == kungfu::GameTransitionType::Ended) {
            m_particleSystem.spawnExplosion({400.0f, 500.0f}, Color{255, 215, 0, 255}); 
        }
    });

    m_whiteScore = 39;
    m_blackScore = 39;
    m_isPaused = m_isHovering = m_selectedPieceAnim.isJumping = m_aiThinking = false;
    m_wasMatchStarted = false;
    m_selectedPieceAnim.jumpTimer = m_pauseTransitionProgress = 0.0f;
    m_particleSystem.clear();
    m_whiteHistory = m_blackHistory = {"Connected"};

    m_pauseButton = std::make_unique<Button>(Vector2D{500.0f, 25.0f}, Vector2D{140.0f, 50.0f}, "Pause", [this]() { togglePause(); });
    m_pauseButton->setColors(m_theme.buttonNormal, m_theme.buttonHover, {255, 255, 255, 255});

    m_cancelMatchmakingButton = std::make_unique<Button>(Vector2D{310.0f, 530.0f}, Vector2D{180.0f, 45.0f}, "Cancel Search", [this]() {
        if (m_networkPlayer) {
            m_networkPlayer->handleDisconnect();
        }
        m_screenManager.popScreen();
    });
    m_cancelMatchmakingButton->setColors({180, 50, 65, 255}, {210, 65, 80, 255}, {255, 255, 255, 255});

    m_eventBus->publish(kungfu::GameTransitionEvent{kungfu::GameTransitionType::Started, kungfu::PlayerColor::White});
}

void ChessGameScreen::spawnCaptureExplosion(const kungfu::Position &boardPos, kungfu::PlayerColor attackerColor) {
    float cellWidth = m_boardRangeX / 8.0f, cellHeight = m_boardRangeY / 8.0f;
    float px = m_boardStartX + boardPos.col() * cellWidth + cellWidth / 2.0f;
    float py = m_boardStartY + boardPos.row() * cellHeight + cellHeight / 2.0f;
    Color targetColor = (attackerColor == kungfu::PlayerColor::White) ? Color{55, 55, 60, 255} : Color{245, 245, 240, 255};
    m_particleSystem.spawnExplosion({px, py}, targetColor);
}

void ChessGameScreen::togglePause() {
    if (m_isNetworkMode) return;
    m_isPaused = !m_isPaused;
    m_pauseButton = std::make_unique<Button>(Vector2D{500.0f, 25.0f}, Vector2D{140.0f, 50.0f}, m_isPaused ? "Resume" : "Pause", [this]() { togglePause(); });
    if (m_isPaused)
        m_pauseButton->setColors({40, 110, 75, 255}, {55, 140, 95, 255}, {255, 255, 255, 255});
    else
        m_pauseButton->setColors(m_theme.buttonNormal, m_theme.buttonHover, {255, 255, 255, 255});
}

void ChessGameScreen::initializeScreen() {
    m_theme.background = Color{18, 19, 23, 255};
    m_theme.titleText = Color{240, 200, 80, 255};
    m_theme.buttonNormal = Color{35, 37, 45, 255};
    m_theme.buttonHover = Color{48, 120, 192, 255};
    m_theme.border = Color{55, 58, 70, 255};
    m_theme.bodyText = Color{210, 215, 225, 255};

    resetGame();

    m_sidebarRestartButton = std::make_unique<Button>(Vector2D{660.0f, 25.0f}, Vector2D{140.0f, 50.0f}, "Restart", [this]() { resetGame(); });
    m_sidebarRestartButton->setColors({48, 120, 192, 255}, {60, 140, 220, 255}, {255, 255, 255, 255});

    m_sidebarMenuButton = std::make_unique<Button>(Vector2D{820.0f, 25.0f}, Vector2D{140.0f, 50.0f}, "Quit Menu", [this]() { m_screenManager.popScreen(); });
    m_sidebarMenuButton->setColors({180, 50, 65, 255}, {210, 65, 80, 255}, {255, 255, 255, 255});

    m_rematchButton = std::make_unique<Button>(Vector2D{220.0f, 540.0f}, Vector2D{160.0f, 50.0f}, "Rematch", [this]() {
        if (m_isNetworkMode && m_networkPlayer) {
            m_networkPlayer->resetMatchState();
            if (m_authSession) {
                m_networkPlayer->beginPlay(false, 0, m_networkPlayer->onlineRoomCode());
            } else {
                m_networkPlayer->connectAndJoin();
            }
            resetGame();
        } else {
            resetGame();
        }
    });
    m_menuButton = std::make_unique<Button>(Vector2D{420.0f, 540.0f}, Vector2D{160.0f, 50.0f}, "Main Menu", [this]() { m_screenManager.popScreen(); });

    m_rematchButton->setColors({40, 110, 75, 255}, {55, 140, 95, 255}, {255, 255, 255, 255});
    m_menuButton->setColors({50, 50, 60, 255}, {70, 70, 85, 255}, {255, 255, 255, 255});
}

void ChessGameScreen::drawOverlays(IRenderer &renderer, const kungfu::view::GameSnapshot &snapshot) {
    if (m_isNetworkMode && m_networkPlayer && !m_networkPlayer->isSpectator() && !m_networkPlayer->hasMatchStarted()) {
        m_overlaysView.drawMatchmaking(renderer, m_searchTimer, m_totalTime, m_networkPlayer->onlineRoomCode(),
                                      *m_cancelMatchmakingButton, m_boardStartX, m_boardStartY, m_boardRangeX, m_boardRangeY);
        return;
    }

    if (m_matchFoundBannerTimer > 0.0f) {
        m_overlaysView.drawMatchFoundBanner(renderer, m_matchFoundBannerTimer, m_networkPlayer);
    }

    if (m_pauseTransitionProgress > 0.0f && !snapshot.isGameOver) {
        m_overlaysView.drawPauseOverlay(renderer, m_pauseTransitionProgress, m_boardStartX, m_boardStartY, m_boardRangeX, m_boardRangeY);
    }

    if (m_isNetworkMode && m_networkPlayer && m_networkPlayer->isOpponentDisconnectedWithCountdown()) {
        m_overlaysView.drawDisconnectCountdown(renderer, m_networkPlayer->opponentDisconnectCountdown());
    }

    if (snapshot.isGameOver) {
        m_overlaysView.drawGameOverOverlay(renderer, determineWinnerColor(snapshot), m_isNetworkMode,
                                           m_networkPlayer, m_isAiOpponent, *m_rematchButton, *m_menuButton,
                                           m_boardStartX, m_boardStartY, m_boardRangeX, m_boardRangeY);
    }
}

void ChessGameScreen::handleJump(const kungfu::Position &pos) {
    m_gameEngine->requestMove(pos, pos);
    m_humanPlayer->clearSelection();
}

void ChessGameScreen::handleBoardClick(int row, int col) {
    BoardPos clickedTile{row, col};
    float timeSinceLastClick = m_totalTime - m_lastClickTime;

    if (m_isNetworkMode && m_networkPlayer) {
        if (!m_networkPlayer->hasMatchStarted() || m_networkPlayer->isOpponentDisconnectedWithCountdown())
            return;

        auto clickedColorOpt = m_gameEngine->getPieceColorAt({row, col});
        if (!m_humanPlayer->selectedPosition().has_value() && clickedColorOpt.has_value()) {
            if (clickedColorOpt.value() != m_networkPlayer->assignedColor())
                return;
        }
    } else if (m_isAiOpponent && !m_humanPlayer->selectedPosition().has_value()) {
        if (m_gameEngine->getPieceColorAt({row, col}) == kungfu::PlayerColor::Black)
            return;
    }

    if (clickedTile == m_lastClickedTile && timeSinceLastClick < 0.22f && timeSinceLastClick > 0.001f) {
        m_lastClickTime = 0.0f;
        m_lastClickedTile = {-1, -1};
        if (auto selectedOpt = m_humanPlayer->selectedPosition()) {
            if (m_isNetworkMode)
                m_networkPlayer->sendMoveToServer(kungfu::PlayerAction(*selectedOpt, *selectedOpt));
            else
                handleJump(*selectedOpt);
        }
        m_isHovering = m_selectedPieceAnim.isJumping = false;
        m_selectedPieceAnim.jumpTimer = 0.0f;
    } else {
        m_lastClickTime = m_totalTime;
        m_lastClickedTile = clickedTile;
        auto selectedBefore = m_humanPlayer->selectedPosition();

        if (m_isNetworkMode) {
            if (selectedBefore.has_value()) {
                kungfu::Position from = selectedBefore.value();
                kungfu::Position to(row, col);

                if (from == to) {
                    m_humanPlayer->clearSelection();
                } else {
                    auto boardNonConst = std::const_pointer_cast<kungfu::IBoard>(m_gameEngine->getBoard());
                    kungfu::RuleEngine validator(boardNonConst);
                    if (validator.validateMove(from, to).isValid) {
                        m_networkPlayer->sendMoveToServer(kungfu::PlayerAction(from, to));
                    }
                    m_humanPlayer->clearSelection();
                }
                
                m_selectedPieceAnim.isJumping = false;
                m_selectedPieceAnim.jumpTimer = 0.0f;
                m_lastClickedTile = {-1, -1};
            } else {
                const int cellSize = kungfu::InputConfig::kDefaultCellSize;
                const int halfCell = cellSize / 2;
                m_humanPlayer->handleClick(col * cellSize + halfCell, row * cellSize + halfCell);
            }
        } else {
            auto activeColor = m_gameEngine->currentTurn();
            kungfu::PieceType movingPieceType = selectedBefore ? getPieceTypeAt(*selectedBefore) : kungfu::PieceType::Pawn;
            if (selectedBefore) {
                if (auto p = m_gameEngine->getBoard()->pieceAt(*selectedBefore))
                    activeColor = p.value()->color();
            }

            auto result = m_humanPlayer->handleClick(col * 100 + 50, row * 100 + 50);
            auto selectedAfter = m_humanPlayer->selectedPosition();

            m_selectedPieceAnim.isJumping = (!selectedBefore && selectedAfter);
            m_isHovering = false;
            m_selectedPieceAnim.jumpTimer = 0.0f;
        }
    }
}

void ChessGameScreen::processAiTurn(float deltaTime) {
    if (m_isPaused || m_gameEngine->isGameOver() || !m_isAiOpponent || !m_aiPlayer)
        return;
    if (!m_config.allowSimultaneousMovement && m_gameEngine->currentTurn() == kungfu::PlayerColor::White) {
        m_aiActionPending = false;
        return;
    }

    bool shouldAiMove = false;
    if (!m_config.allowSimultaneousMovement) {
        if (m_gameEngine->currentTurn() == kungfu::PlayerColor::Black && !m_aiActionPending && !m_gameEngine->getArbiter().hasActiveMotion()) {
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
        auto snapshot = kungfu::view::SnapshotBuilder::build(*m_gameEngine->getBoard(), m_gameEngine->getArbiter(), m_gameEngine->getCurrentTimeMs(), m_gameEngine->isGameOver(), std::nullopt);
        m_aiFuture = std::async(std::launch::async, [this, snapshot]() { return m_aiPlayer->decideActions(snapshot); });
    }

    if (m_aiThinking && m_aiFuture.valid()) {
        if (m_aiFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            auto aiRequests = m_aiFuture.get();
            m_aiThinking = m_aiActionPending = false;

            if (!aiRequests.empty()) {
                auto aiResults = m_gameEngine->processActionRequests(aiRequests);
                if (!aiResults.empty() && aiResults.front().status == kungfu::ActionStatus::Accepted) {
                    if (!m_config.allowSimultaneousMovement)
                        m_gameEngine->wait(1000);
                }
            }
        }
    }
}

void ChessGameScreen::drawContent(IRenderer &renderer) {
    auto board = m_gameEngine->getBoard();
    bool isGameOver = m_gameEngine->isGameOver();
    if (m_isNetworkMode && m_networkPlayer) {
        if (m_networkPlayer->matchEnded() || m_networkPlayer->opponentDisconnected()) {
            isGameOver = true;
        }
    }

    auto snapshot = kungfu::view::SnapshotBuilder::build(*board, m_gameEngine->getArbiter(), m_gameEngine->getCurrentTimeMs(), isGameOver, m_humanPlayer->selectedPosition());

    m_boardView.draw(renderer, snapshot, m_gameEngine->getPremoveQueue(), m_gameEngine->getCurrentTimeMs(), m_boardStartX, m_boardStartY, m_boardRangeX, m_boardRangeY, m_hoveredTile.row, m_hoveredTile.col, m_isHovering, m_selectedPieceAnim.isJumping, m_selectedPieceAnim.jumpTimer, m_isPaused, m_config.allowSimultaneousMovement);
    m_particleSystem.draw(renderer);
    m_headerView.draw(renderer, "KUNG-FU CHESS", m_theme.background, m_theme.border, m_theme.titleText, *m_pauseButton, *m_sidebarRestartButton, *m_sidebarMenuButton);
    m_sidebarView.draw(renderer, m_whiteHistory, m_blackHistory, m_theme.background, m_theme.border);

    int whiteAbsolute = calculateAbsoluteMaterialScore(snapshot, kungfu::PlayerColor::White);
    int blackAbsolute = calculateAbsoluteMaterialScore(snapshot, kungfu::PlayerColor::Black);

    std::string whiteName = getPlayerName(kungfu::PlayerColor::White);
    std::string blackName = getPlayerName(kungfu::PlayerColor::Black);

    m_footerView.draw(renderer, whiteName, whiteAbsolute, blackName, blackAbsolute, isGameOver, m_isPaused, m_config.allowSimultaneousMovement, m_gameEngine->currentTurn(), m_theme.background, m_theme.border);
    drawOverlays(renderer, snapshot);
}

void ChessGameScreen::onEnter() {}

void ChessGameScreen::onExit() {
    if (m_soundPlayer) {
        m_soundPlayer->stopSound("walk");
    }
}

void ChessGameScreen::update(float deltaTime) {
    tickBackground(deltaTime);
    m_pauseButton->update(deltaTime);
    m_sidebarRestartButton->update(deltaTime);
    m_sidebarMenuButton->update(deltaTime);

    m_totalTime += deltaTime;
    m_pauseTransitionProgress = std::clamp(m_pauseTransitionProgress + (m_isPaused ? deltaTime * 5.0f : -deltaTime * 5.0f), 0.0f, 1.0f);
    m_particleSystem.update(deltaTime);

    if (m_isNetworkMode && m_networkPlayer && !m_networkPlayer->isSpectator() && !m_networkPlayer->hasMatchStarted()) {
        m_searchTimer += deltaTime;
        m_cancelMatchmakingButton->update(deltaTime);
    }

    if (m_matchFoundBannerTimer > 0.0f) {
        m_matchFoundBannerTimer -= deltaTime;
    }

    if (m_isPaused) {
        if (m_soundPlayer) m_soundPlayer->stopSound("walk");
        return;
    }

    m_gameEngine->wait(static_cast<int>(deltaTime * 1000.0f));

    if (m_soundPlayer) {
        if (!m_gameEngine->isGameOver() && m_gameEngine->getArbiter().hasActiveMotion())
            m_soundPlayer->playLoop("walk");
        else
            m_soundPlayer->stopSound("walk");
    }

    bool isGameOver = m_gameEngine->isGameOver();
    if (m_isNetworkMode && m_networkPlayer) {
        if (m_networkPlayer->matchEnded() || m_networkPlayer->opponentDisconnected()) {
            isGameOver = true;
        }
    }

    if (isGameOver) {
        m_rematchButton->update(deltaTime);
        m_menuButton->update(deltaTime);

        auto board = m_gameEngine->getBoard();
        auto snapshot = kungfu::view::SnapshotBuilder::build(*board, m_gameEngine->getArbiter(), m_gameEngine->getCurrentTimeMs(), isGameOver, std::nullopt);
        auto winner = determineWinnerColor(snapshot);

        if (winner.has_value()) {
            bool isLocalWinner = false;
            if (m_isNetworkMode && m_networkPlayer) {
                isLocalWinner = (winner.value() == m_networkPlayer->assignedColor());
            } else if (m_isAiOpponent) {
                isLocalWinner = (winner.value() == kungfu::PlayerColor::White);
            } else {
                isLocalWinner = true;
            }

            if (isLocalWinner) {
                if (std::rand() % 6 == 0) {
                    float px = 200.0f + (static_cast<float>(std::rand()) / RAND_MAX) * 400.0f;
                    float py = 350.0f + (static_cast<float>(std::rand()) / RAND_MAX) * 150.0f;
                    m_particleSystem.spawnExplosion({px, py}, Color{255, 215, 0, 180});
                }
            }
        } else if (m_isNetworkMode && m_networkPlayer && (m_networkPlayer->matchEnded() || m_networkPlayer->opponentDisconnected())) {
            if (std::rand() % 6 == 0) {
                float px = 200.0f + (static_cast<float>(std::rand()) / RAND_MAX) * 400.0f;
                float py = 350.0f + (static_cast<float>(std::rand()) / RAND_MAX) * 150.0f;
                m_particleSystem.spawnExplosion({px, py}, Color{255, 215, 0, 180});
            }
        }
    }

    if (m_isNetworkMode && m_networkPlayer) {
        if (!m_wasMatchStarted && m_networkPlayer->hasMatchStarted()) {
            m_wasMatchStarted = true;
            m_matchFoundBannerTimer = 3.5f;
            if (m_soundPlayer) {
                m_soundPlayer->playSound("move");
            }
        }

        if (m_networkPlayer->hasPendingSync()) {
            std::string syncedBoard = m_networkPlayer->consumePendingSync();
            applySyncedBoard(syncedBoard);
        }

        auto results = m_networkPlayer->pollResults();
        for (const auto &res : results) {
            if (res.status == kungfu::ActionStatus::Rejected) {
                addHistoryLog(m_networkPlayer->assignedColor(), "Move rejected by server");
            }
        }

        auto snapshot = kungfu::view::SnapshotBuilder::build(*m_gameEngine->getBoard(), m_gameEngine->getArbiter(), m_gameEngine->getCurrentTimeMs(), isGameOver, std::nullopt);
        auto networkActions = m_networkPlayer->decideActions(snapshot);
        for (const auto &req : networkActions) {
            m_gameEngine->applyServerMove(req.action.from, req.action.to);
        }
    } else {
        processAiTurn(deltaTime);
    }

    auto selectedOpt = m_humanPlayer->selectedPosition();
    if (selectedOpt.has_value() && (m_selectedPieceAnim.isJumping || m_isHovering))
        m_selectedPieceAnim.jumpTimer += deltaTime;
    else
        m_selectedPieceAnim.jumpTimer = 0.0f;
}

void ChessGameScreen::handleInput(const std::vector<InputEvent> &events) {
    for (const auto &event : events) {
        if (event.type == InputEvent::Type::Mouse) {
            const auto &mouse = event.mouse;

            if (m_isNetworkMode && m_networkPlayer && !m_networkPlayer->isSpectator() && !m_networkPlayer->hasMatchStarted()) {
                m_cancelMatchmakingButton->handleInput(mouse);
                return;
            }

            m_pauseButton->handleInput(mouse);
            m_sidebarRestartButton->handleInput(mouse);
            m_sidebarMenuButton->handleInput(mouse);

            bool isGameOver = m_gameEngine->isGameOver();
            if (m_isNetworkMode && m_networkPlayer) {
                if (m_networkPlayer->matchEnded() || m_networkPlayer->opponentDisconnected()) {
                    isGameOver = true;
                }
            }

            if (isGameOver) {
                m_rematchButton->handleInput(mouse);
                m_menuButton->handleInput(mouse);
            } else if (!m_isPaused && (!m_isNetworkMode || !m_networkPlayer || !m_networkPlayer->isSpectator())) {
                if (mouse.logicalX >= m_boardStartX && mouse.logicalX < m_boardStartX + m_boardRangeX &&
                    mouse.logicalY >= m_boardStartY && mouse.logicalY < m_boardStartY + m_boardRangeY) {

                    int col = static_cast<int>(mouse.logicalX / 100.0f);
                    int row = static_cast<int>((mouse.logicalY - m_boardStartY) / 100.0f);

                    if (col >= 0 && col < 8 && row >= 0 && row < 8) {
                        m_hoveredTile = BoardPos{row, col};
                        if (mouse.action == MouseEvent::Action::Press && mouse.button == MouseButton::Left) {
                            handleBoardClick(row, col);
                        }
                    }
                } else {
                    m_hoveredTile = BoardPos{-1, -1};
                }
            }
        } else if (event.type == InputEvent::Type::Keyboard) {
            bool isGameOver = m_gameEngine->isGameOver();
            if (m_isNetworkMode && m_networkPlayer) {
                if (m_networkPlayer->matchEnded() || m_networkPlayer->opponentDisconnected()) {
                    isGameOver = true;
                }
            }

            if (event.key.key == Key::Escape) {
                if (isGameOver)
                    m_screenManager.popScreen();
                else
                    togglePause();
            } else if (event.key.key == Key::Space) {
                if (!isGameOver)
                    togglePause();
            }
        }
    }
}