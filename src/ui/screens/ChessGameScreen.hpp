// ui/screens/ChessGameScreen.hpp
#pragma once

#include "ui/screens/BaseScreen.hpp"
#include "ui/framework/ScreenManager.hpp"
#include "ui/framework/ISoundPlayer.hpp"
#include "ui/components/Button.hpp"
#include "ui/components/ParticleSystem.hpp"
#include "ui/components/SidebarView.hpp"
#include "ui/components/HeaderView.hpp"
#include "ui/components/FooterView.hpp"
#include "ui/components/BoardView.hpp"
#include "engine/core/GameEngine.hpp"
#include "engine/events/IGameObserver.hpp"
#include "players/human/HumanPlayer.hpp"
#include "engine/snapshot/SnapshotBuilder.hpp"
#include "engine/io/BoardParser.hpp"
#include "engine/common/PieceTokenCodec.hpp"
#include "ui/framework/EventBus.hpp"
#include "engine/events/GameEvents.hpp"
#include "players/IPlayer.hpp"
#include "players/ai/GenericAIPlayer.hpp"
#include "players/ai/ClassicMinimaxStrategy.hpp"
#include "players/ai/RealTimeStrategies.hpp"
#include "players/network/NetworkPlayer.hpp"
#include "engine/common/PieceValues.hpp"
#include "engine/common/BoardPresets.hpp" 
#include <future>
#include <memory>
#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <thread>
#include <mutex>
#include <optional>

class ChessGameScreen : public BaseScreen {
public:
    enum class AiDifficulty { Easy, Medium, Hard };

private:
    std::shared_ptr<kungfu::GameEngine> m_gameEngine;
    std::shared_ptr<kungfu::HumanPlayer> m_humanPlayer;
    std::shared_ptr<kungfu::IPlayer> m_aiPlayer;
    kungfu::GameConfig m_config;
    std::shared_ptr<ISoundPlayer> m_soundPlayer;
    std::shared_ptr<kungfu::EventBus> m_eventBus;

    int m_whiteScore = 39;
    int m_blackScore = 39;

    bool m_isNetworkMode = false;
    std::shared_ptr<kungfu::NetworkPlayer> m_networkPlayer;
    boost::asio::io_context m_ioContext;
    std::thread m_networkThread;

    bool m_isPaused = false;
    bool m_isAiOpponent = false;
    AiDifficulty m_aiDifficulty = AiDifficulty::Medium;
    float m_aiDecisionTimer = 1.0f;
    bool m_aiActionPending = false;
    bool m_aiThinking = false;
    std::future<std::vector<kungfu::ActionRequest>> m_aiFuture;

    std::vector<std::string> m_whiteHistory, m_blackHistory;
    std::unique_ptr<Button> m_pauseButton, m_sidebarRestartButton, m_sidebarMenuButton, m_rematchButton, m_menuButton;

    const float m_boardStartX = 0.0f, m_boardStartY = 100.0f, m_boardRangeX = 800.0f, m_boardRangeY = 800.0f;

    struct BoardPos {
        int row = -1, col = -1;
        bool isValid() const { return row >= 0 && row < 8 && col >= 0 && col < 8; }
        bool operator==(const BoardPos &o) const { return row == o.row && col == o.col; }
        bool operator!=(const BoardPos &o) const { return !(*this == o); }
    };

    struct PieceAnimation {
        bool isJumping = false;
        float jumpTimer = 0.0f;
    };

    float m_totalTime = 0.0f, m_lastClickTime = 0.0f;
    BoardPos m_lastClickedTile{-1, -1}, m_hoveredTile{-1, -1};
    bool m_isHovering = false;
    PieceAnimation m_selectedPieceAnim;
    float m_pauseTransitionProgress = 0.0f;

    ParticleSystem m_particleSystem;
    SidebarView m_sidebarView;
    HeaderView m_headerView;
    FooterView m_footerView;
    BoardView m_boardView;

    kungfu::PieceType getPieceTypeAt(const kungfu::Position& pos) const {
        if (auto p = m_gameEngine->getBoard()->pieceAt(pos)) return p.value()->type();
        if (auto p = m_gameEngine->getArbiter().getPieceInTransitAt(pos)) return p.value()->type();
        return kungfu::PieceType::Pawn;
    }

    // Precise absolute calculation of the total value of all living pieces for each side out of 39
    int calculateAbsoluteMaterialScore(const kungfu::view::GameSnapshot& snapshot, kungfu::PlayerColor color) const {
        int total = 0;
        for (const auto& piece : snapshot.pieces) {
            if (piece.color == color && piece.state != kungfu::PieceState::Captured) {
                total += kungfu::PieceValues::getStandardValue(piece.type);
            }
        }
        return total;
    }

    // Identify the winning player based on the presence of kings on the board
    std::optional<kungfu::PlayerColor> determineWinnerColor(const kungfu::view::GameSnapshot& snapshot) const {
        bool hasWhiteKing = false;
        bool hasBlackKing = false;
        for (const auto& piece : snapshot.pieces) {
            if (piece.type == kungfu::PieceType::King && piece.state != kungfu::PieceState::Captured) {
                if (piece.color == kungfu::PlayerColor::White) hasWhiteKing = true;
                if (piece.color == kungfu::PlayerColor::Black) hasBlackKing = true;
            }
        }
        if (hasWhiteKing && !hasBlackKing) return kungfu::PlayerColor::White;
        if (!hasWhiteKing && hasBlackKing) return kungfu::PlayerColor::Black;
        return std::nullopt;
    }

    std::string getMoveNotationString(kungfu::PieceType type, const BoardPos &from, const BoardPos &to) const {
        char pieceChar = kungfu::PieceTokenCodec::toChar(type);
        std::string notation = { pieceChar, ':', ' ', static_cast<char>('a' + to.col), static_cast<char>('1' + to.row) };
        if (from == to) notation += " (Jump)";
        return notation;
    }

    void addHistoryLog(kungfu::PlayerColor color, const std::string& logText) {
        auto& history = (color == kungfu::PlayerColor::White) ? m_whiteHistory : m_blackHistory;
        history.push_back(logText);
        if (history.size() > 8) history.erase(history.begin());
    }

    std::unique_ptr<kungfu::IAIDecisionStrategy> createAiStrategy() const {
        if (m_config.allowSimultaneousMovement) {
            if (m_aiDifficulty == AiDifficulty::Easy) return std::make_unique<kungfu::RealTimeEasyStrategy>();
            if (m_aiDifficulty == AiDifficulty::Medium) return std::make_unique<kungfu::RealTimeMediumStrategy>();
            return std::make_unique<kungfu::RealTimeHardStrategy>();
        }
        int depth = (m_aiDifficulty == AiDifficulty::Easy) ? 1 : (m_aiDifficulty == AiDifficulty::Medium) ? 2 : 3;
        return std::make_unique<kungfu::ClassicMinimaxStrategy>(depth);
    }

    void applySyncedBoard(const std::string& boardText) {
    auto board = kungfu::BoardParser::parse(boardText);
    auto ruleEngine = std::make_shared<kungfu::RuleEngine>(board);

    // Re-create the game engine using the synchronized board snapshot
    m_gameEngine = std::make_shared<kungfu::GameEngine>(board, ruleEngine, m_config, std::make_shared<kungfu::ChessPromotionRule>(), m_eventBus);
    m_humanPlayer = std::make_shared<kungfu::HumanPlayer>(m_gameEngine);
    m_humanPlayer->setCellSize(kungfu::InputConfig::kDefaultCellSize);
    
    std::cout << "[Spectator] Board successfully synchronized with live room!" << std::endl;
}
    void resetGame() {
        auto startBoard = kungfu::BoardParser::parse(kungfu::BoardPresets::kStandardStartBoard);

        m_eventBus = std::make_shared<kungfu::EventBus>();
        auto board = kungfu::BoardParser::parse(startBoard);
        
        m_gameEngine = std::make_shared<kungfu::GameEngine>(
            board, 
            std::make_shared<kungfu::RuleEngine>(board), 
            m_config,
            std::make_shared<kungfu::ChessPromotionRule>(),
            m_eventBus
        );
        
        m_humanPlayer = std::make_shared<kungfu::HumanPlayer>(m_gameEngine);
        m_aiPlayer = m_isAiOpponent ? std::make_shared<kungfu::GenericAIPlayer>(kungfu::PlayerColor::Black, createAiStrategy()) : nullptr;
        m_humanPlayer->setCellSize(kungfu::InputConfig::kDefaultCellSize);

        m_eventBus->subscribe<kungfu::MoveCompletedEvent>([this](const kungfu::MoveCompletedEvent& ev) {
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

        m_eventBus->subscribe<kungfu::PlaySoundEvent>([this](const kungfu::PlaySoundEvent& ev) {
            if (m_soundPlayer) {
                m_soundPlayer->playSound(ev.soundId);
            }
        });

        m_eventBus->subscribe<kungfu::GameTransitionEvent>([this](const kungfu::GameTransitionEvent& ev) {
            if (ev.type == kungfu::GameTransitionType::Ended) {
                m_particleSystem.spawnExplosion({400.0f, 500.0f}, Color{255, 215, 0, 255}); 
            }
        });

        m_whiteScore = 39;
        m_blackScore = 39;
        m_isPaused = m_isHovering = m_selectedPieceAnim.isJumping = m_aiThinking = false;
        m_selectedPieceAnim.jumpTimer = m_pauseTransitionProgress = 0.0f;
        m_particleSystem.clear();
        m_whiteHistory = m_blackHistory = {"Connected"};

        m_pauseButton = std::make_unique<Button>(Vector2D{500.0f, 25.0f}, Vector2D{140.0f, 50.0f}, "Pause", [this]() { togglePause(); });
        m_pauseButton->setColors(m_theme.buttonNormal, m_theme.buttonHover, {255, 255, 255, 255});

        m_eventBus->publish(kungfu::GameTransitionEvent{kungfu::GameTransitionType::Started, kungfu::PlayerColor::White});
    }

    void spawnCaptureExplosion(const kungfu::Position &boardPos, kungfu::PlayerColor attackerColor) {
        float cellWidth = m_boardRangeX / 8.0f, cellHeight = m_boardRangeY / 8.0f;
        float px = m_boardStartX + boardPos.col() * cellWidth + cellWidth / 2.0f;
        float py = m_boardStartY + boardPos.row() * cellHeight + cellHeight / 2.0f;
        Color targetColor = (attackerColor == kungfu::PlayerColor::White) ? Color{55, 55, 60, 255} : Color{245, 245, 240, 255};
        m_particleSystem.spawnExplosion({px, py}, targetColor);
    }

    void togglePause() {
        if (m_isNetworkMode) return;
        m_isPaused = !m_isPaused;
        m_pauseButton = std::make_unique<Button>(Vector2D{500.0f, 25.0f}, Vector2D{140.0f, 50.0f}, m_isPaused ? "Resume" : "Pause", [this]() { togglePause(); });
        if (m_isPaused) m_pauseButton->setColors({40, 110, 75, 255}, {55, 140, 95, 255}, {255, 255, 255, 255});
        else m_pauseButton->setColors(m_theme.buttonNormal, m_theme.buttonHover, {255, 255, 255, 255});
    }

    void initializeScreen() {
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

        m_rematchButton = std::make_unique<Button>(Vector2D{220.0f, 540.0f}, Vector2D{160.0f, 50.0f}, "Rematch", [this]() { resetGame(); });
        m_menuButton = std::make_unique<Button>(Vector2D{420.0f, 540.0f}, Vector2D{160.0f, 50.0f}, "Main Menu", [this]() { m_screenManager.popScreen(); });

        m_rematchButton->setColors({40, 110, 75, 255}, {55, 140, 95, 255}, {255, 255, 255, 255});
        m_menuButton->setColors({50, 50, 60, 255}, {70, 70, 85, 255}, {255, 255, 255, 255});
    }

    void drawOverlays(IRenderer &renderer, const kungfu::view::GameSnapshot &snapshot) {
        if (m_pauseTransitionProgress > 0.0f && !snapshot.isGameOver) {
            renderer.drawRectangle({m_boardStartX, m_boardStartY}, {m_boardRangeX, m_boardRangeY}, {15, 15, 20, static_cast<std::uint8_t>(m_pauseTransitionProgress * 180)}, true);
            float panelY = -200.0f + 500.0f * (m_pauseTransitionProgress * m_pauseTransitionProgress * (3.0f - 2.0f * m_pauseTransitionProgress));
            renderer.drawRectangle({350.0f, panelY}, {300.0f, 150.0f}, {25, 25, 35, static_cast<std::uint8_t>(m_pauseTransitionProgress * 240)}, true);
            renderer.drawRectangle({350.0f, panelY}, {300.0f, 150.0f}, {80, 80, 100, static_cast<std::uint8_t>(m_pauseTransitionProgress * 255)}, false);
            renderer.drawText("PAUSED", {445.0f, panelY + 50.0f}, 38, {240, 200, 80, static_cast<std::uint8_t>(m_pauseTransitionProgress * 255)});
            renderer.drawText("Press SPACE or Resume", {390.0f, panelY + 110.0f}, 14, {180, 180, 190, static_cast<std::uint8_t>(m_pauseTransitionProgress * 255)});
        }

        if (m_isNetworkMode && m_networkPlayer && m_networkPlayer->isOpponentDisconnectedWithCountdown()) {
            int seconds = m_networkPlayer->opponentDisconnectCountdown();
            renderer.drawRectangle({150.0f, 350.0f}, {500.0f, 150.0f}, {20, 20, 25, 230}, true);
            renderer.drawRectangle({150.0f, 350.0f}, {500.0f, 150.0f}, {220, 60, 60, 255}, false);
            renderer.drawText("OPPONENT DISCONNECTED", {230.0f, 400.0f}, 24, {220, 60, 60, 255});
            renderer.drawText("Auto-Resign in: " + std::to_string(seconds) + " seconds", {280.0f, 450.0f}, 16, {255, 255, 255, 255});
        }

        // ====================================================
        // Draw an advanced and visually impressive feedback window for game over
        // ====================================================
        if (snapshot.isGameOver) {
            renderer.drawRectangle({m_boardStartX, m_boardStartY}, {m_boardRangeX, m_boardRangeY}, {10, 10, 15, 160}, true);
            
            // Identify the winner
            auto winnerOpt = determineWinnerColor(snapshot);
            bool drawVictoryText = false;
            bool drawDefeatText = false;
            std::string neutralResultText = "MATCH ENDED";

            if (winnerOpt.has_value()) {
                if (m_isNetworkMode && m_networkPlayer) {
                    if (winnerOpt.value() == m_networkPlayer->assignedColor()) drawVictoryText = true;
                    else drawDefeatText = true;
                } else if (m_isAiOpponent) {
                    if (winnerOpt.value() == kungfu::PlayerColor::White) drawVictoryText = true;
                    else drawDefeatText = true;
                } else {
                    neutralResultText = (winnerOpt.value() == kungfu::PlayerColor::White) ? "WHITE WINS!" : "BLACK WINS!";
                }
            } else {
                neutralResultText = "IT'S A DRAW!";
            }

            // The announcement panel styled in the center
            Vector2D panelPos{150.0f, 320.0f};
            Vector2D panelSize{500.0f, 320.0f};

            // Bottom shadow
            renderer.drawRectangle({panelPos.x + 8.0f, panelPos.y + 8.0f}, panelSize, {0, 0, 0, 120}, true);

            if (drawVictoryText) {
                // Glowing gold background for victory
                renderer.drawRectangle(panelPos, panelSize, {30, 35, 45, 245}, true);
                renderer.drawRectangle(panelPos, panelSize, {240, 200, 80, 255}, false);
                renderer.drawText("VICTORY!", {panelPos.x + 130.0f, panelPos.y + 110.0f}, 52, {240, 200, 80, 255});
                renderer.drawText("An epic real-time match!", {panelPos.x + 155.0f, panelPos.y + 160.0f}, 14, {180, 185, 200, 255});
            } else if (drawDefeatText) {
                // Maroon background for defeat
                renderer.drawRectangle(panelPos, panelSize, {25, 20, 25, 245}, true);
                renderer.drawRectangle(panelPos, panelSize, {210, 60, 60, 255}, false);
                renderer.drawText("DEFEAT", {panelPos.x + 155.0f, panelPos.y + 110.0f}, 52, {210, 60, 60, 255});
                renderer.drawText("Defeat is a step to mastery.", {panelPos.x + 145.0f, panelPos.y + 160.0f}, 14, {160, 160, 170, 255});
            } else {
                // Neutral metallic gray background for local play or draw
                renderer.drawRectangle(panelPos, panelSize, {25, 27, 35, 245}, true);
                renderer.drawRectangle(panelPos, panelSize, {100, 105, 120, 255}, false);
                renderer.drawText(neutralResultText, {panelPos.x + 110.0f, panelPos.y + 110.0f}, 42, {220, 225, 235, 255});
                renderer.drawText("Good game!", {panelPos.x + 205.0f, panelPos.y + 160.0f}, 14, {160, 165, 175, 255});
            }

            m_rematchButton->draw(renderer);
            m_menuButton->draw(renderer);
        }
    }

    void handleJump(const kungfu::Position& pos) {
        m_gameEngine->requestMove(pos, pos); 
        m_humanPlayer->clearSelection();
    }

    void handleBoardClick(int row, int col) {
        BoardPos clickedTile{row, col};
        float timeSinceLastClick = m_totalTime - m_lastClickTime;

        if (m_isNetworkMode && m_networkPlayer) {
            auto clickedColorOpt = m_gameEngine->getPieceColorAt({row, col});
            if (!m_humanPlayer->selectedPosition().has_value() && clickedColorOpt.has_value()) {
                if (clickedColorOpt.value() != m_networkPlayer->assignedColor()) return;
            }
        } else if (m_isAiOpponent && !m_humanPlayer->selectedPosition().has_value()) {
            if (m_gameEngine->getPieceColorAt({row, col}) == kungfu::PlayerColor::Black) return;
        }

        if (clickedTile == m_lastClickedTile && timeSinceLastClick < 0.48f && timeSinceLastClick > 0.001f) {
            m_lastClickTime = 0.0f;
            m_lastClickedTile = {-1, -1};
            if (auto selectedOpt = m_humanPlayer->selectedPosition()) {
                if (m_isNetworkMode) m_networkPlayer->sendMoveToServer(kungfu::PlayerAction(*selectedOpt, *selectedOpt));
                else handleJump(*selectedOpt);
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
                } else {
                    const int cellSize = kungfu::InputConfig::kDefaultCellSize;
                    const int halfCell = cellSize / 2;
                    m_humanPlayer->handleClick(col * cellSize + halfCell, row * cellSize + halfCell);
                }
            } else {
                auto activeColor = m_gameEngine->currentTurn();
                kungfu::PieceType movingPieceType = selectedBefore ? getPieceTypeAt(*selectedBefore) : kungfu::PieceType::Pawn;
                if (selectedBefore) {
                    if (auto p = m_gameEngine->getBoard()->pieceAt(*selectedBefore)) activeColor = p.value()->color();
                }

                auto result = m_humanPlayer->handleClick(col * 100 + 50, row * 100 + 50);
                auto selectedAfter = m_humanPlayer->selectedPosition();

                m_selectedPieceAnim.isJumping = (!selectedBefore && selectedAfter);
                m_isHovering = false;
                m_selectedPieceAnim.jumpTimer = 0.0f;
            }
        }
    }

    void processAiTurn(float deltaTime) {
        if (m_isPaused || m_gameEngine->isGameOver() || !m_isAiOpponent || !m_aiPlayer) return;
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
                        if (!m_config.allowSimultaneousMovement) m_gameEngine->wait(1000);
                    }
                }
            }
        }
    }

protected:
    void drawContent(IRenderer &renderer) override {
        auto board = m_gameEngine->getBoard();
        auto snapshot = kungfu::view::SnapshotBuilder::build(*board, m_gameEngine->getArbiter(), m_gameEngine->getCurrentTimeMs(), m_gameEngine->isGameOver(), m_humanPlayer->selectedPosition());

        m_boardView.draw(renderer, snapshot, m_gameEngine->getPremoveQueue(), m_gameEngine->getCurrentTimeMs(), m_boardStartX, m_boardStartY, m_boardRangeX, m_boardRangeY, m_hoveredTile.row, m_hoveredTile.col, m_isHovering, m_selectedPieceAnim.isJumping, m_selectedPieceAnim.jumpTimer, m_isPaused, m_config.allowSimultaneousMovement);
        m_particleSystem.draw(renderer);
        m_headerView.draw(renderer, "KUNG-FU CHESS", m_theme.background, m_theme.border, m_theme.titleText, *m_pauseButton, *m_sidebarRestartButton, *m_sidebarMenuButton);
        m_sidebarView.draw(renderer, m_whiteHistory, m_blackHistory, m_theme.background, m_theme.border);
        
        // Calculate and inject the absolute score (number of living material points out of 39) into the bottom panel
        int whiteAbsolute = calculateAbsoluteMaterialScore(snapshot, kungfu::PlayerColor::White);
        int blackAbsolute = calculateAbsoluteMaterialScore(snapshot, kungfu::PlayerColor::Black);

        m_footerView.draw(renderer, whiteAbsolute, blackAbsolute, m_gameEngine->isGameOver(), m_isPaused, m_config.allowSimultaneousMovement, m_gameEngine->currentTurn(), m_theme.background, m_theme.border);
        drawOverlays(renderer, snapshot);
    }

public:
    explicit ChessGameScreen(ScreenManager &manager, 
                             bool isSimultaneousMode, 
                             bool isAiOpponent, 
                             AiDifficulty aiDifficulty, 
                             std::shared_ptr<ISoundPlayer> soundPlayer = std::make_shared<NullSoundPlayer>(), 
                             bool isNetworkMode = false, 
                             std::string host = "127.0.0.1", 
                             std::string port = "8080",
                             bool isSpectator = false,
                             std::uint64_t spectateMatchId = 0,
                             std::uint64_t onlineRoomCode = 0)
        : BaseScreen(manager, "Chess Match"), m_isAiOpponent(isAiOpponent), m_aiDifficulty(aiDifficulty), m_soundPlayer(std::move(soundPlayer)), m_isNetworkMode(isNetworkMode) {
        
        m_config.allowSimultaneousMovement = isSimultaneousMode;
        if (!m_config.allowSimultaneousMovement) {
            m_config.cooldownDurationMs = 0;
            m_config.allowJumping = m_config.enablePremoves = false;
        }
        initializeScreen();

        if (m_isNetworkMode) {
            m_isAiOpponent = false;
            m_aiPlayer = nullptr;
            
            // Pass the isSpectator and spectateMatchId details directly to the NetworkPlayer!
            m_networkPlayer = std::make_shared<kungfu::NetworkPlayer>(m_ioContext, host, port, isSpectator, spectateMatchId, onlineRoomCode);
            m_networkPlayer->connectAndJoin();
            
            m_networkThread = std::thread([this]() {
                boost::asio::io_context::work work(m_ioContext);
                m_ioContext.run();
            });
        }
    }


    explicit ChessGameScreen(ScreenManager &manager, bool isSimultaneousMode, bool isAiOpponent = false, std::shared_ptr<ISoundPlayer> soundPlayer = std::make_shared<NullSoundPlayer>())
        : ChessGameScreen(manager, isSimultaneousMode, isAiOpponent, AiDifficulty::Medium, soundPlayer, false, "127.0.0.1", "8080") {}

    ~ChessGameScreen() override {
        if (m_isNetworkMode) {
            m_ioContext.stop();
            if (m_networkThread.joinable()) m_networkThread.join();
        }
    }

    void onEnter() override {}
    void onExit() override {}

    void update(float deltaTime) override {
        tickBackground(deltaTime);
        m_pauseButton->update(deltaTime);
        m_sidebarRestartButton->update(deltaTime);
        m_sidebarMenuButton->update(deltaTime);

        m_totalTime += deltaTime;
        m_pauseTransitionProgress = std::clamp(m_pauseTransitionProgress + (m_isPaused ? deltaTime * 5.0f : -deltaTime * 5.0f), 0.0f, 1.0f);
        m_particleSystem.update(deltaTime);

        if (m_isPaused) {
            if (m_soundPlayer) m_soundPlayer->stopSound("walk");
            return;
        }

        m_gameEngine->wait(static_cast<int>(deltaTime * 1000.0f));

        if (m_soundPlayer) {
            if (!m_gameEngine->isGameOver() && m_gameEngine->getArbiter().hasActiveMotion()) m_soundPlayer->playLoop("walk");
            else m_soundPlayer->stopSound("walk");
        }

        if (m_gameEngine->isGameOver()) {
            m_rematchButton->update(deltaTime);
            m_menuButton->update(deltaTime);

            auto board = m_gameEngine->getBoard();
            auto snapshot = kungfu::view::SnapshotBuilder::build(*board, m_gameEngine->getArbiter(), m_gameEngine->getCurrentTimeMs(), m_gameEngine->isGameOver(), std::nullopt);
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
            }
        }

        if (m_isNetworkMode && m_networkPlayer) {
            // Pulling the room's initial board data for the viewer/re-joiner
            if (m_networkPlayer->hasPendingSync()) {
                std::string syncedBoard = m_networkPlayer->consumePendingSync();
                applySyncedBoard(syncedBoard);
            }

            auto results = m_networkPlayer->pollResults();
            for (const auto& res : results) {
                if (res.status == kungfu::ActionStatus::Rejected) {
                    addHistoryLog(m_networkPlayer->assignedColor(), "Move rejected by server");
                }
            }

            auto snapshot = kungfu::view::SnapshotBuilder::build(*m_gameEngine->getBoard(), m_gameEngine->getArbiter(), m_gameEngine->getCurrentTimeMs(), m_gameEngine->isGameOver(), std::nullopt);
            auto networkActions = m_networkPlayer->decideActions(snapshot);
            for (const auto& req : networkActions) {
                m_gameEngine->requestMove(req.action.from, req.action.to); 
            }
        } else {
            processAiTurn(deltaTime);
        }

        auto selectedOpt = m_humanPlayer->selectedPosition();
        if (selectedOpt.has_value() && (m_selectedPieceAnim.isJumping || m_isHovering)) m_selectedPieceAnim.jumpTimer += deltaTime;
        else m_selectedPieceAnim.jumpTimer = 0.0f;
    }

    void handleInput(const std::vector<InputEvent> &events) override {
    for (const auto &event : events) {
        if (event.type == InputEvent::Type::Mouse) {
            const auto &mouse = event.mouse;
            m_pauseButton->handleInput(mouse);
            m_sidebarRestartButton->handleInput(mouse);
            m_sidebarMenuButton->handleInput(mouse);

            if (m_gameEngine->isGameOver()) {
                m_rematchButton->handleInput(mouse);
                m_menuButton->handleInput(mouse);
            } 
            // Only process board clicks if NOT a spectator
            else if (!m_isPaused && (!m_isNetworkMode || !m_networkPlayer || !m_networkPlayer->isSpectator())) {
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
            if (event.key.key == Key::Escape) {
                if (m_gameEngine->isGameOver()) m_screenManager.popScreen();
                else togglePause();
            } else if (event.key.key == Key::Space) {
                if (!m_gameEngine->isGameOver()) togglePause();
            }
        }
    }
}
};