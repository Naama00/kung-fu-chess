// ui/screens/ChessGameScreen.cpp
#include "ui/screens/ChessGameScreen.hpp"
#include "engine/common/PieceValues.hpp"
#include "engine/events/GameEvents.hpp"
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
    : BaseScreen(manager, "Chess Match"),
      m_soundPlayer(std::move(soundPlayer))
{
    kungfu::GameConfig config;
    config.allowSimultaneousMovement = isSimultaneousMode;

    m_matchController = std::make_unique<kungfu::MatchController>(
        config,
        isSimultaneousMode,
        isAiOpponent,
        aiDifficulty,
        isNetworkMode,
        std::move(host),
        std::move(port),
        isSpectator,
        spectateMatchId,
        onlineRoomCode
    );

    initializeScreen();
}

ChessGameScreen::ChessGameScreen(ScreenManager &manager,
                                 bool isSimultaneousMode,
                                 std::shared_ptr<ISoundPlayer> soundPlayer,
                                 std::shared_ptr<kungfu::NetworkSession> authSession,
                                 std::uint64_t onlineRoomCode)
    : BaseScreen(manager, "Chess Match"),
      m_soundPlayer(std::move(soundPlayer))
{
    kungfu::GameConfig config;
    config.allowSimultaneousMovement = isSimultaneousMode;

    m_matchController = std::make_unique<kungfu::MatchController>(
        config,
        isSimultaneousMode,
        false,
        AiDifficulty::Medium,
        true,
        kungfu::ClientConfig::kDefaultHost, 
        kungfu::ClientConfig::kDefaultPort, 
        false, 0, onlineRoomCode,
        authSession
    );

    initializeScreen();
}

ChessGameScreen::ChessGameScreen(ScreenManager &manager, bool isSimultaneousMode, bool isAiOpponent, std::shared_ptr<ISoundPlayer> soundPlayer)
    : ChessGameScreen(manager, isSimultaneousMode, isAiOpponent, AiDifficulty::Medium, std::move(soundPlayer), false, 
                        kungfu::ClientConfig::kDefaultHost, kungfu::ClientConfig::kDefaultPort) {}

void ChessGameScreen::subscribeToEvents() {
    if (!m_matchController) return;
    auto eventBus = m_matchController->getEventBus();
    if (!eventBus) return;

    eventBus->subscribe<kungfu::MoveCompletedEvent>([this](const kungfu::MoveCompletedEvent &ev) {
        if (ev.detail.cancelled || !ev.detail.piece) return;
        if (ev.detail.isCapture) {
            spawnCaptureExplosion(ev.detail.to, ev.detail.piece->color());
        }
    });

    eventBus->subscribe<kungfu::PlaySoundEvent>([this](const kungfu::PlaySoundEvent &ev) {
        if (m_soundPlayer) {
            m_soundPlayer->playSound(ev.soundId);
        }
    });

    eventBus->subscribe<kungfu::GameTransitionEvent>([this](const kungfu::GameTransitionEvent &ev) {
        if (ev.type == kungfu::GameTransitionType::Ended) {
            m_particleSystem.spawnExplosion({400.0f, 500.0f}, Color{255, 215, 0, 255});
        }
    });
}

void ChessGameScreen::initializeScreen() {
    m_theme.background = Color{18, 19, 23, 255};
    m_theme.titleText = Color{240, 200, 80, 255};
    m_theme.buttonNormal = Color{35, 37, 45, 255};
    m_theme.buttonHover = Color{48, 120, 192, 255};
    m_theme.border = Color{55, 58, 70, 255};
    m_theme.bodyText = Color{210, 215, 225, 255};

    subscribeToEvents();

    m_pauseButton = std::make_unique<Button>(Vector2D{500.0f, 25.0f}, Vector2D{140.0f, 50.0f}, "Pause", [this]() {
        if (m_matchController) {
            m_matchController->togglePause();
            bool paused = m_matchController->isPaused();
            m_pauseButton = std::make_unique<Button>(Vector2D{500.0f, 25.0f}, Vector2D{140.0f, 50.0f}, paused ? "Resume" : "Pause", [this]() {
                m_matchController->togglePause();
            });
            if (paused) {
                m_pauseButton->setColors({40, 110, 75, 255}, {55, 140, 95, 255}, {255, 255, 255, 255});
            } else {
                m_pauseButton->setColors(m_theme.buttonNormal, m_theme.buttonHover, {255, 255, 255, 255});
            }
        }
    });
    m_pauseButton->setColors(m_theme.buttonNormal, m_theme.buttonHover, {255, 255, 255, 255});

    m_sidebarRestartButton = std::make_unique<Button>(Vector2D{660.0f, 25.0f}, Vector2D{140.0f, 50.0f}, "Restart", [this]() {
        if (m_matchController) {
            m_matchController->resetMatch();
            subscribeToEvents();
        }
    });
    m_sidebarRestartButton->setColors({48, 120, 192, 255}, {60, 140, 220, 255}, {255, 255, 255, 255});

    m_sidebarMenuButton = std::make_unique<Button>(Vector2D{820.0f, 25.0f}, Vector2D{140.0f, 50.0f}, "Quit Menu", [this]() {
        m_screenManager.popScreen();
    });
    m_sidebarMenuButton->setColors({180, 50, 65, 255}, {210, 65, 80, 255}, {255, 255, 255, 255});

    m_rematchButton = std::make_unique<Button>(Vector2D{220.0f, 540.0f}, Vector2D{160.0f, 50.0f}, "Rematch", [this]() {
        if (m_matchController) {
            m_matchController->resetMatch();
            subscribeToEvents();
        }
    });
    m_rematchButton->setColors({40, 110, 75, 255}, {55, 140, 95, 255}, {255, 255, 255, 255});

    m_menuButton = std::make_unique<Button>(Vector2D{420.0f, 540.0f}, Vector2D{160.0f, 50.0f}, "Main Menu", [this]() {
        m_screenManager.popScreen();
    });
    m_menuButton->setColors({50, 50, 60, 255}, {70, 70, 85, 255}, {255, 255, 255, 255});

    m_cancelMatchmakingButton = std::make_unique<Button>(Vector2D{310.0f, 530.0f}, Vector2D{180.0f, 45.0f}, "Cancel Search", [this]() {
        if (m_matchController) {
            m_matchController->cancelMatchmaking();
        }
        m_screenManager.popScreen();
    });
    m_cancelMatchmakingButton->setColors({180, 50, 65, 255}, {210, 65, 80, 255}, {255, 255, 255, 255});
}

void ChessGameScreen::spawnCaptureExplosion(const kungfu::Position &boardPos, kungfu::PlayerColor attackerColor) {
    float cellWidth = m_boardRangeX / 8.0f;
    float cellHeight = m_boardRangeY / 8.0f;
    float px = m_boardStartX + boardPos.col() * cellWidth + cellWidth / 2.0f;
    float py = m_boardStartY + boardPos.row() * cellHeight + cellHeight / 2.0f;
    Color targetColor = (attackerColor == kungfu::PlayerColor::White) ? Color{55, 55, 60, 255} : Color{245, 245, 240, 255};
    m_particleSystem.spawnExplosion({px, py}, targetColor);
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

void ChessGameScreen::drawOverlays(IRenderer &renderer, const kungfu::view::GameSnapshot &snapshot) {
    if (m_matchController->isNetworkMode() && !m_matchController->isSpectator() && !m_matchController->hasMatchStarted()) {
        m_overlaysView.drawMatchmaking(renderer, m_matchController->searchTimer(), m_totalTime,
                                       m_matchController->onlineRoomCode(),
                                       *m_cancelMatchmakingButton, m_boardStartX, m_boardStartY, m_boardRangeX, m_boardRangeY);
        return;
    }

    if (m_matchFoundBannerTimer > 0.0f) {
        m_overlaysView.drawMatchFoundBanner(renderer, m_matchFoundBannerTimer, m_matchController->networkPlayer());
    }

    if (m_pauseTransitionProgress > 0.0f && !snapshot.isGameOver) {
        m_overlaysView.drawPauseOverlay(renderer, m_pauseTransitionProgress, m_boardStartX, m_boardStartY, m_boardRangeX, m_boardRangeY);
    }

    if (m_matchController->isOpponentDisconnectedWithCountdown()) {
        m_overlaysView.drawDisconnectCountdown(renderer, m_matchController->opponentDisconnectCountdown());
    }

    if (snapshot.isGameOver) {
        m_overlaysView.drawGameOverOverlay(renderer, m_matchController->determineWinnerColor(), m_matchController->isNetworkMode(),
                                           m_matchController->networkPlayer(), m_matchController->isAiOpponent(), *m_rematchButton, *m_menuButton,
                                           m_boardStartX, m_boardStartY, m_boardRangeX, m_boardRangeY);
    }
}

void ChessGameScreen::handleBoardClick(int row, int col) {
    BoardPos clickedTile{row, col};
    float timeSinceLastClick = m_totalTime - m_lastClickTime;

    if (clickedTile == m_lastClickedTile && timeSinceLastClick < 0.22f && timeSinceLastClick > 0.001f) {
        m_lastClickTime = 0.0f;
        m_lastClickedTile = {-1, -1};
        if (auto selectedOpt = m_matchController->selectedPosition()) {
            m_matchController->handleJump(*selectedOpt);
        }
        m_isHovering = m_selectedPieceAnim.isJumping = false;
        m_selectedPieceAnim.jumpTimer = 0.0f;
    } else {
        m_lastClickTime = m_totalTime;
        m_lastClickedTile = clickedTile;
        auto selectedBefore = m_matchController->selectedPosition();

        m_matchController->handleTileClick(row, col);

        auto selectedAfter = m_matchController->selectedPosition();
        m_selectedPieceAnim.isJumping = (!selectedBefore && selectedAfter);
        m_isHovering = false;
        m_selectedPieceAnim.jumpTimer = 0.0f;
    }
}

void ChessGameScreen::drawContent(IRenderer &renderer) {
    auto snapshot = m_matchController->getSnapshot();

    m_boardView.draw(renderer, snapshot, m_matchController->getPremoveQueue(), m_matchController->getCurrentTimeMs(),
                     m_boardStartX, m_boardStartY, m_boardRangeX, m_boardRangeY,
                     m_hoveredTile.row, m_hoveredTile.col, m_isHovering,
                     m_selectedPieceAnim.isJumping, m_selectedPieceAnim.jumpTimer,
                     m_matchController->isPaused(), true);

    m_particleSystem.draw(renderer);
    m_headerView.draw(renderer, "KUNG-FU CHESS", m_theme.background, m_theme.border, m_theme.titleText,
                      *m_pauseButton, *m_sidebarRestartButton, *m_sidebarMenuButton);

    const auto &whiteHistory = m_matchController->getHistory(kungfu::PlayerColor::White);
    const auto &blackHistory = m_matchController->getHistory(kungfu::PlayerColor::Black);
    m_sidebarView.draw(renderer, whiteHistory, blackHistory, m_theme.background, m_theme.border);

    int whiteAbsolute = calculateAbsoluteMaterialScore(snapshot, kungfu::PlayerColor::White);
    int blackAbsolute = calculateAbsoluteMaterialScore(snapshot, kungfu::PlayerColor::Black);

    std::string whiteName = m_matchController->getPlayerName(kungfu::PlayerColor::White);
    std::string blackName = m_matchController->getPlayerName(kungfu::PlayerColor::Black);

    m_footerView.draw(renderer, whiteName, whiteAbsolute, blackName, blackAbsolute,
                      snapshot.isGameOver, m_matchController->isPaused(), true,
                      kungfu::PlayerColor::White, m_theme.background, m_theme.border);

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
    m_pauseTransitionProgress = std::clamp(m_pauseTransitionProgress + (m_matchController->isPaused() ? deltaTime * 5.0f : -deltaTime * 5.0f), 0.0f, 1.0f);
    m_particleSystem.update(deltaTime);

    if (m_matchController->isNetworkMode() && !m_matchController->isSpectator() && !m_matchController->hasMatchStarted()) {
        m_cancelMatchmakingButton->update(deltaTime);
    }

    if (m_matchFoundBannerTimer > 0.0f) {
        m_matchFoundBannerTimer -= deltaTime;
    }

    if (m_matchController->isNetworkMode()) {
        if (!m_wasMatchStarted && m_matchController->hasMatchStarted()) {
            m_wasMatchStarted = true;
            m_matchFoundBannerTimer = 3.5f;
            if (m_soundPlayer) {
                m_soundPlayer->playSound("move");
            }
        }
    }

    if (m_matchController->isPaused()) {
        if (m_soundPlayer) m_soundPlayer->stopSound("walk");
        return;
    }

    m_matchController->update(deltaTime);

    if (m_soundPlayer) {
        if (!m_matchController->isGameOver() && m_matchController->hasActiveMotion()) {
            m_soundPlayer->playLoop("walk");
        } else {
            m_soundPlayer->stopSound("walk");
        }
    }

    if (m_matchController->isGameOver()) {
        m_rematchButton->update(deltaTime);
        m_menuButton->update(deltaTime);

        auto winner = m_matchController->determineWinnerColor();
        if (winner.has_value()) {
            bool isLocalWinner = false;
            if (m_matchController->isNetworkMode() && m_matchController->networkPlayer()) {
                isLocalWinner = (winner.value() == m_matchController->networkPlayer()->assignedColor());
            } else if (m_matchController->isAiOpponent()) {
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
        } else if (m_matchController->isNetworkMode() && (m_matchController->matchEnded() || m_matchController->opponentDisconnected())) {
            if (std::rand() % 6 == 0) {
                float px = 200.0f + (static_cast<float>(std::rand()) / RAND_MAX) * 400.0f;
                float py = 350.0f + (static_cast<float>(std::rand()) / RAND_MAX) * 150.0f;
                m_particleSystem.spawnExplosion({px, py}, Color{255, 215, 0, 180});
            }
        }
    }

    auto selectedOpt = m_matchController->selectedPosition();
    if (selectedOpt.has_value() && (m_selectedPieceAnim.isJumping || m_isHovering)) {
        m_selectedPieceAnim.jumpTimer += deltaTime;
    } else {
        m_selectedPieceAnim.jumpTimer = 0.0f;
    }
}

void ChessGameScreen::handleInput(const std::vector<InputEvent> &events) {
    for (const auto &event : events) {
        if (event.type == InputEvent::Type::Mouse) {
            const auto &mouse = event.mouse;

            if (m_matchController->isNetworkMode() && !m_matchController->isSpectator() && !m_matchController->hasMatchStarted()) {
                m_cancelMatchmakingButton->handleInput(mouse);
                return;
            }

            m_pauseButton->handleInput(mouse);
            m_sidebarRestartButton->handleInput(mouse);
            m_sidebarMenuButton->handleInput(mouse);

            bool gameOver = m_matchController->isGameOver();

            if (gameOver) {
                m_rematchButton->handleInput(mouse);
                m_menuButton->handleInput(mouse);
            } else if (!m_matchController->isPaused() && !m_matchController->isSpectator()) {
                if (mouse.logicalX >= m_boardStartX && mouse.logicalX < m_boardStartX + m_boardRangeX &&
                    mouse.logicalY >= m_boardStartY && mouse.logicalY < m_boardStartY + m_boardRangeY) {

                    int col = static_cast<int>(mouse.logicalX / static_cast<float>(kungfu::InputConfig::kDefaultCellSize));
                    int row = static_cast<int>((mouse.logicalY - m_boardStartY) / static_cast<float>(kungfu::InputConfig::kDefaultCellSize)); 

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
            bool gameOver = m_matchController->isGameOver();

            if (event.key.key == Key::Escape) {
                if (gameOver) {
                    m_screenManager.popScreen();
                } else {
                    if (m_matchController) {
                        m_matchController->togglePause();
                    }
                }
            } else if (event.key.key == Key::Space) {
                if (!gameOver && m_matchController) {
                    m_matchController->togglePause();
                }
            }
        }
    }
}