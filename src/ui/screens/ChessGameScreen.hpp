#pragma once

#include "ui/framework/BaseScreen.hpp"
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
#include "players/ai/AIPlayer.hpp"
#include "engine/snapshot/SnapshotBuilder.hpp"
#include "engine/io/BoardParser.hpp"
#include "engine/common/PieceTokenCodec.hpp"
#include <memory>
#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>

class ChessGameScreen : public BaseScreen
{
private:
    std::shared_ptr<kungfu::GameEngine> m_gameEngine;
    std::shared_ptr<kungfu::HumanPlayer> m_humanPlayer;
    std::shared_ptr<kungfu::AIPlayer> m_aiPlayer;
    kungfu::GameConfig m_config;
    std::shared_ptr<ISoundPlayer> m_soundPlayer;
    bool m_isPaused = false;
    bool m_isAiOpponent = false;    // שדה חדש למעקב אחר משחק מול ה-AI
    float m_aiDecisionTimer = 1.0f; // טיימר אקראי לפעולת ה-AI במצב סימולטני

    // היסטוריית מהלכים של כל שחקן
    std::vector<std::string> m_whiteHistory;
    std::vector<std::string> m_blackHistory;

    // כפתורי בקרה
    std::unique_ptr<Button> m_pauseButton;
    std::unique_ptr<Button> m_sidebarRestartButton;
    std::unique_ptr<Button> m_sidebarMenuButton;

    // כפתורי סוף המשחק
    std::unique_ptr<Button> m_rematchButton;
    std::unique_ptr<Button> m_menuButton;

    // פריסת הלוח (בתוך מרחב 1000x1000 הלוגי)
    const float m_boardStartX = 0.0f;
    const float m_boardStartY = 100.0f;
    const float m_boardRangeX = 800.0f;
    const float m_boardRangeY = 800.0f;

    struct BoardPos
    {
        int row = -1;
        int col = -1;
        bool isValid() const { return row >= 0 && row < 8 && col >= 0 && col < 8; }
        bool operator==(const BoardPos &other) const { return row == other.row && col == other.col; }
        bool operator!=(const BoardPos &other) const { return !(*this == other); }
    };

    struct PieceAnimation
    {
        bool isJumping = false;
        float jumpTimer = 0.0f;
    };

    float m_totalTime = 0.0f;
    float m_lastClickTime = 0.0f;
    BoardPos m_lastClickedTile;
    bool m_isHovering = false;
    PieceAnimation m_selectedPieceAnim;

    BoardPos m_hoveredTile{-1, -1};
    float m_pauseTransitionProgress = 0.0f;

    // רכיבי UI מודולריים
    ParticleSystem m_particleSystem;
    SidebarView m_sidebarView;
    HeaderView m_headerView;
    FooterView m_footerView;
    BoardView m_boardView;

    int getPieceValue(kungfu::PieceType type) const
    {
        switch (type)
        {
        case kungfu::PieceType::Queen:
            return 9;
        case kungfu::PieceType::Rook:
            return 5;
        case kungfu::PieceType::Bishop:
            return 3;
        case kungfu::PieceType::Knight:
            return 3;
        case kungfu::PieceType::Pawn:
            return 1;
        default:
            return 0;
        }
    }

    int calculatePlayerScore(kungfu::PlayerColor color) const
    {
        int total = 0;
        if (!m_gameEngine || !m_gameEngine->getBoard())
        {
            return 0;
        }

        for (const auto &piece : m_gameEngine->getBoard()->pieces())
        {
            if (piece && piece->color() == color && piece->state() != kungfu::PieceState::Captured)
            {
                total += getPieceValue(piece->type());
            }
        }
        for (const auto &motion : m_gameEngine->getArbiter().activeMotions())
        {
            auto piece = motion.piece();
            if (piece && piece->color() == color && piece->state() == kungfu::PieceState::Airborne)
            {
                total += getPieceValue(piece->type());
            }
        }
        return total;
    }

    // פונקציית עזר ליצירת מחרוזת ייצוג מהלך תקין
    std::string getMoveNotationString(kungfu::PieceType type, const BoardPos &from, const BoardPos &to) const
    {
        char pieceChar = kungfu::PieceTokenCodec::toChar(type);
        char fileChar = 'a' + to.col;
        char rankChar = '1' + to.row;

        std::string notation = "";
        notation += pieceChar;
        notation += ": ";
        notation += fileChar;
        notation += rankChar;

        if (from == to)
        {
            notation += " (Jump)";
        }
        return notation;
    }

    void resetGame()
    {
        std::string startBoard =
            "bR bN bB bQ bK bB bN bR\n"
            "bP bP bP bP bP bP bP bP\n"
            ". . . . . . . .\n"
            ". . . . . . . .\n"
            ". . . . . . . .\n"
            ". . . . . . . .\n"
            "wP wP wP wP wP wP wP wP\n"
            "wR wN wB wQ wK wB wN wR\n";

        auto board = kungfu::BoardParser::parse(startBoard);
        auto ruleEngine = std::make_shared<kungfu::RuleEngine>(board);

        m_gameEngine = std::make_shared<kungfu::GameEngine>(board, ruleEngine, m_config);
        m_humanPlayer = std::make_shared<kungfu::HumanPlayer>(m_gameEngine);

        // יצירת ה-AI רק במידה והשחקן בחר לשחק מול המחשב
        if (m_isAiOpponent)
        {
            m_aiPlayer = std::make_shared<kungfu::AIPlayer>(kungfu::PlayerColor::Black);
        }
        else
        {
            m_aiPlayer = nullptr;
        }

        m_humanPlayer->setCellSize(100);

        struct GameEventObserver : public kungfu::IGameObserver
        {
            std::shared_ptr<ISoundPlayer> player;
            ChessGameScreen *screen;

            GameEventObserver(std::shared_ptr<ISoundPlayer> p, ChessGameScreen *s)
                : player(std::move(p)), screen(s) {}

            void onMoveCompleted(const kungfu::ArrivalEvent &event, int /*currentTimeMs*/) override
            {
                if (event.cancelled || !event.piece)
                {
                    return;
                }
                if (event.capturedKing)
                {
                    player->playSound("game_over");
                }
                else if (event.isCapture)
                {
                    player->playSound("capture");
                }

                if (event.isCapture && screen)
                {
                    screen->spawnCaptureExplosion(event.to, event.piece->color());
                }
            }
        };

        if (m_soundPlayer)
        {
            m_gameEngine->addObserver(std::make_shared<GameEventObserver>(m_soundPlayer, this));
        }

        m_isPaused = false;
        m_isHovering = false;
        m_selectedPieceAnim.isJumping = false;
        m_selectedPieceAnim.jumpTimer = 0.0f;
        m_pauseTransitionProgress = 0.0f;
        m_particleSystem.clear();
        m_whiteHistory.clear();
        m_blackHistory.clear();
        m_whiteHistory.push_back("Connected");
        m_blackHistory.push_back("Connected");

        m_pauseButton = std::make_unique<Button>(
            Vector2D{500.0f, 25.0f}, Vector2D{140.0f, 50.0f}, "Pause",
            [this]()
            { togglePause(); });
        m_pauseButton->setColors(m_theme.buttonNormal, m_theme.buttonHover, {255, 255, 255, 255});
    }

    void spawnCaptureExplosion(const kungfu::Position &boardPos, kungfu::PlayerColor attackerColor)
    {
        float cellWidth = m_boardRangeX / 8.0f;
        float cellHeight = m_boardRangeY / 8.0f;

        float px = m_boardStartX + boardPos.col() * cellWidth + cellWidth / 2.0f;
        float py = m_boardStartY + boardPos.row() * cellHeight + cellHeight / 2.0f;

        Color targetColor = (attackerColor == kungfu::PlayerColor::White)
                                ? Color{55, 55, 60, 255}
                                : Color{245, 245, 240, 255};

        m_particleSystem.spawnExplosion({px, py}, targetColor);
    }

    void togglePause()
    {
        m_isPaused = !m_isPaused;

        if (m_isPaused)
        {
            m_pauseButton = std::make_unique<Button>(
                Vector2D{500.0f, 25.0f}, Vector2D{140.0f, 50.0f}, "Resume",
                [this]()
                { togglePause(); });
            m_pauseButton->setColors({40, 110, 75, 255}, {55, 140, 95, 255}, {255, 255, 255, 255});
        }
        else
        {
            m_pauseButton = std::make_unique<Button>(
                Vector2D{500.0f, 25.0f}, Vector2D{140.0f, 50.0f}, "Pause",
                [this]()
                { togglePause(); });
            m_pauseButton->setColors(m_theme.buttonNormal, m_theme.buttonHover, {255, 255, 255, 255});
        }
    }

    void initializeScreen()
    {
        m_theme.background = Color{18, 19, 23, 255};
        m_theme.titleText = Color{240, 200, 80, 255};
        m_theme.buttonNormal = Color{35, 37, 45, 255};
        m_theme.buttonHover = Color{48, 120, 192, 255};
        m_theme.border = Color{55, 58, 70, 255};
        m_theme.bodyText = Color{210, 215, 225, 255};

        resetGame();

        m_sidebarRestartButton = std::make_unique<Button>(
            Vector2D{660.0f, 25.0f}, Vector2D{140.0f, 50.0f}, "Restart",
            [this]()
            { resetGame(); });
        m_sidebarRestartButton->setColors({48, 120, 192, 255}, {60, 140, 220, 255}, {255, 255, 255, 255});

        m_sidebarMenuButton = std::make_unique<Button>(
            Vector2D{820.0f, 25.0f}, Vector2D{140.0f, 50.0f}, "Quit Menu",
            [this]()
            { m_screenManager.popScreen(); });
        m_sidebarMenuButton->setColors({180, 50, 65, 255}, {210, 65, 80, 255}, {255, 255, 255, 255});

        m_rematchButton = std::make_unique<Button>(
            Vector2D{220.0f, 540.0f}, Vector2D{160.0f, 50.0f}, "Rematch",
            [this]()
            { resetGame(); });
        m_menuButton = std::make_unique<Button>(
            Vector2D{420.0f, 540.0f}, Vector2D{160.0f, 50.0f}, "Main Menu",
            [this]()
            { m_screenManager.popScreen(); });

        m_rematchButton->setColors({40, 110, 75, 255}, {55, 140, 95, 255}, {255, 255, 255, 255});
        m_menuButton->setColors({50, 50, 60, 255}, {70, 70, 85, 255}, {255, 255, 255, 255});
    }

    void drawOverlays(IRenderer &renderer, const kungfu::view::GameSnapshot &snapshot)
    {
        if (m_pauseTransitionProgress > 0.0f && !snapshot.isGameOver)
        {
            Color dimColor{15, 15, 20, static_cast<std::uint8_t>(m_pauseTransitionProgress * 180)};
            renderer.drawRectangle({m_boardStartX, m_boardStartY}, {m_boardRangeX, m_boardRangeY}, dimColor, true);

            float t = m_pauseTransitionProgress;
            float smoothT = t * t * (3.0f - 2.0f * t);

            float startY = -200.0f;
            float endY = 300.0f;
            float panelY = startY + (endY - startY) * smoothT;

            Color panelBg{25, 25, 35, static_cast<std::uint8_t>(m_pauseTransitionProgress * 240)};
            Color panelBorder{80, 80, 100, static_cast<std::uint8_t>(m_pauseTransitionProgress * 255)};
            Color textColor{240, 200, 80, static_cast<std::uint8_t>(m_pauseTransitionProgress * 255)};
            Color subTextColor{180, 180, 190, static_cast<std::uint8_t>(m_pauseTransitionProgress * 255)};

            renderer.drawRectangle({350.0f, panelY}, {300.0f, 150.0f}, panelBg, true);
            renderer.drawRectangle({350.0f, panelY}, {300.0f, 150.0f}, panelBorder, false);
            renderer.drawText("PAUSED", {445.0f, panelY + 50.0f}, 38, textColor);
            renderer.drawText("Press SPACE or Resume", {390.0f, panelY + 110.0f}, 14, subTextColor);
        }

        if (snapshot.isGameOver)
        {
            renderer.drawRectangle({m_boardStartX, m_boardStartY}, {m_boardRangeX, m_boardRangeY}, {10, 10, 15, 150}, true);
            renderer.drawRectangle({150.0f, 350.0f}, {500.0f, 300.0f}, {20, 20, 25, 240}, true);
            renderer.drawRectangle({150.0f, 350.0f}, {500.0f, 300.0f}, {200, 60, 60, 255}, false);
            renderer.drawText("GAME OVER", {250.0f, 440.0f}, 50, {255, 60, 60, 255});
            m_rematchButton->draw(renderer);
            m_menuButton->draw(renderer);
        }
    }

protected:
    void drawContent(IRenderer &renderer) override
    {
        auto board = m_gameEngine->getBoard();
        int rows = board->rows();
        int cols = board->cols();

        auto selectedOpt = m_humanPlayer->selectedPosition();

        auto snapshot = kungfu::view::SnapshotBuilder::build(
            *board,
            m_gameEngine->getArbiter(),
            m_gameEngine->getCurrentTimeMs(),
            m_gameEngine->isGameOver(),
            selectedOpt,
            m_boardRangeX / cols);

        m_boardView.draw(renderer, snapshot, m_gameEngine->getPremoveQueue(),
                         m_boardStartX, m_boardStartY, m_boardRangeX, m_boardRangeY,
                         m_hoveredTile.row, m_hoveredTile.col,
                         m_isHovering, m_selectedPieceAnim.isJumping, m_selectedPieceAnim.jumpTimer,
                         m_isPaused, m_config.allowSimultaneousMovement);

        m_particleSystem.draw(renderer);

        m_headerView.draw(renderer, "KUNG-FU CHESS", m_theme.background, m_theme.border, m_theme.titleText,
                          *m_pauseButton, *m_sidebarRestartButton, *m_sidebarMenuButton);

        m_sidebarView.draw(renderer, m_whiteHistory, m_blackHistory, m_theme.background, m_theme.border);

        int whiteScore = calculatePlayerScore(kungfu::PlayerColor::White);
        int blackScore = calculatePlayerScore(kungfu::PlayerColor::Black);
        m_footerView.draw(renderer, whiteScore, blackScore, m_gameEngine->isGameOver(), m_isPaused,
                          m_config.allowSimultaneousMovement, m_gameEngine->currentTurn(),
                          m_theme.background, m_theme.border);

        drawOverlays(renderer, snapshot);
    }

public:
    explicit ChessGameScreen(ScreenManager &manager,
                             bool isSimultaneousMode,
                             bool isAiOpponent = false, // פרמטר חדש
                             std::shared_ptr<ISoundPlayer> soundPlayer = std::make_shared<NullSoundPlayer>())
        : BaseScreen(manager, "Chess Match"), m_isAiOpponent(isAiOpponent), m_soundPlayer(std::move(soundPlayer))
    {
        m_config.allowSimultaneousMovement = isSimultaneousMode;
        if (!m_config.allowSimultaneousMovement)
        {
            m_config.cooldownDurationMs = 0;
            m_config.allowJumping = false;
            m_config.enablePremoves = false;
        }
        initializeScreen();
    }

    explicit ChessGameScreen(ScreenManager &manager,
                             bool isSimultaneousMode,
                             std::shared_ptr<ISoundPlayer> soundPlayer = std::make_shared<NullSoundPlayer>())
        : BaseScreen(manager, "Chess Match"), m_soundPlayer(std::move(soundPlayer))
    {
        m_config.allowSimultaneousMovement = isSimultaneousMode;
        if (!m_config.allowSimultaneousMovement)
        {
            m_config.cooldownDurationMs = 0;
            m_config.allowJumping = false;
            m_config.enablePremoves = false;
        }
        initializeScreen();
    }

    void onEnter() override
    {
        std::cout << "Chess Game Screen Activated!" << std::endl;
    }

    void onExit() override
    {
        std::cout << "Chess Game Screen Deactivated!" << std::endl;
    }

    void update(float deltaTime) override
    {
        m_pauseButton->update(deltaTime);
        m_sidebarRestartButton->update(deltaTime);
        m_sidebarMenuButton->update(deltaTime);

        m_totalTime += deltaTime;

        if (m_isPaused)
        {
            m_pauseTransitionProgress = std::min(1.0f, m_pauseTransitionProgress + deltaTime * 5.0f);
        }
        else
        {
            m_pauseTransitionProgress = std::max(0.0f, m_pauseTransitionProgress - deltaTime * 5.0f);
        }

        m_particleSystem.update(deltaTime);

        if (m_isPaused)
        {
            if (m_soundPlayer)
            {
                m_soundPlayer->stopSound("walk");
            }
            return;
        }

        int ms = static_cast<int>(deltaTime * 1000.0f);
        m_gameEngine->wait(ms);

        if (m_soundPlayer && !m_gameEngine->isGameOver())
        {
            if (m_gameEngine->getArbiter().hasActiveMotion())
            {
                m_soundPlayer->playLoop("walk");
            }
            else
            {
                m_soundPlayer->stopSound("walk");
            }
        }
        else if (m_soundPlayer && m_gameEngine->isGameOver())
        {
            m_soundPlayer->stopSound("walk");
        }

        if (m_gameEngine->isGameOver())
        {
            m_rematchButton->update(deltaTime);
            m_menuButton->update(deltaTime);
        }

        // --- ניהול וקבלת החלטות של ה-AI ---
        if (!m_isPaused && !m_gameEngine->isGameOver() && m_isAiOpponent && m_aiPlayer)
        {
            auto snapshot = kungfu::view::SnapshotBuilder::build(
                *m_gameEngine->getBoard(),
                m_gameEngine->getArbiter(),
                m_gameEngine->getCurrentTimeMs(),
                m_gameEngine->isGameOver(),
                std::nullopt,
                m_boardRangeX / 8);

            bool shouldAiMove = false;

            if (!m_config.allowSimultaneousMovement)
            {
                // מצב קלאסי (תורות): ה-AI זז מיד ברגע שהתור עובר לשחור
                if (m_gameEngine->currentTurn() == kungfu::PlayerColor::Black)
                {
                    shouldAiMove = true;
                }
            }
            else
            {
                // מצב קונג-פו (זמן אמת): ה-AI פועל במרווחי זמן מדומים לקבלת החלטות
                m_aiDecisionTimer -= deltaTime;
                if (m_aiDecisionTimer <= 0.0f)
                {
                    shouldAiMove = true;
                    // איפוס מחדש של הטיימר לזמן אקראי בין 1.0 ל-2.2 שניות (לדימוי שחקן אנושי)
                    m_aiDecisionTimer = 1.0f + (static_cast<float>(std::rand()) / RAND_MAX) * 1.2f;
                }
            }

            if (shouldAiMove)
            {
                auto aiRequests = m_aiPlayer->decideActions(snapshot);
                if (!aiRequests.empty())
                {
                    auto aiResults = m_gameEngine->processActionRequests(aiRequests);
                    if (!aiResults.empty() && aiResults.front().status == kungfu::ActionStatus::Accepted)
                    {

                        // תרגום המהלך וכתיבתו להיסטוריה הצידית של שחור
                        auto action = aiRequests.front().action;
                        kungfu::PieceType pieceType = kungfu::PieceType::Pawn;
                        auto pieceOpt = m_gameEngine->getBoard()->pieceAt(action.from);
                        if (pieceOpt.has_value() && pieceOpt.value())
                        {
                            pieceType = pieceOpt.value()->type();
                        }

                        BoardPos fromPos{action.from.row(), action.from.col()};
                        BoardPos toPos{action.to.row(), action.to.col()};
                        std::string logText = getMoveNotationString(pieceType, fromPos, toPos);

                        m_blackHistory.push_back(logText);
                        if (m_blackHistory.size() > 8)
                        {
                            m_blackHistory.erase(m_blackHistory.begin());
                        }

                        // במצב תורות קלאסי בלבד: קידום הסימולציה ב-1000ms להשלמת ההגעה הפיזית ליעד
                        if (!m_config.allowSimultaneousMovement)
                        {
                            m_gameEngine->wait(1000);
                        }
                    }
                }
            }
        }

        auto selectedOpt = m_humanPlayer->selectedPosition();
        if (selectedOpt.has_value() && (m_selectedPieceAnim.isJumping || m_isHovering))
        {
            m_selectedPieceAnim.jumpTimer += deltaTime;
        }
        else
        {
            m_selectedPieceAnim.jumpTimer = 0.0f;
        }
    }

    void handleInput(const std::vector<InputEvent> &events) override
    {
        for (const auto &event : events)
        {
            if (event.type == InputEvent::Type::Mouse)
            {
                const auto &mouse = event.mouse;

                m_pauseButton->handleInput(mouse);
                m_sidebarRestartButton->handleInput(mouse);
                m_sidebarMenuButton->handleInput(mouse);

                if (m_gameEngine->isGameOver())
                {
                    m_rematchButton->handleInput(mouse);
                    m_menuButton->handleInput(mouse);
                }
                else if (!m_isPaused)
                {
                    if (mouse.logicalX >= m_boardStartX && mouse.logicalX < m_boardStartX + m_boardRangeX &&
                        mouse.logicalY >= m_boardStartY && mouse.logicalY < m_boardStartY + m_boardRangeY)
                    {
                        float boardClickX = mouse.logicalX;
                        float boardClickY = mouse.logicalY - m_boardStartY;

                        int col = static_cast<int>(boardClickX / 100.0f);
                        int row = static_cast<int>(boardClickY / 100.0f);

                        if (col >= 0 && col < 8 && row >= 0 && row < 8)
                        {
                            m_hoveredTile = BoardPos{row, col};

                            if (mouse.action == MouseEvent::Action::Press && mouse.button == MouseButton::Left)
                            {
                                BoardPos clickedTile{row, col};
                                float timeSinceLastClick = m_totalTime - m_lastClickTime;

                                // --- הגנה ארכיטקטונית לחסימת קלט ---
                                // אם המשחק מול ה-AI, והמשתמש מבצע קליק ראשון, נמנע ממנו לבחור בכלים של ה-AI (השחורים)
                                auto selectedBefore = m_humanPlayer->selectedPosition();
                                if (!selectedBefore.has_value() && m_isAiOpponent)
                                {
                                    auto pieceColor = m_gameEngine->getPieceColorAt(kungfu::Position(row, col));
                                    if (pieceColor.has_value() && pieceColor.value() == kungfu::PlayerColor::Black)
                                    {
                                        continue; // מעבר ישיר לאירוע הבא - מניעת המשך העיבוד ב-Controller [1]
                                    }
                                }

                                if (clickedTile == m_lastClickedTile && timeSinceLastClick < 0.48f && timeSinceLastClick > 0.001f)
                                {
                                    m_lastClickTime = 0.0f;
                                    m_lastClickedTile = BoardPos{-1, -1};

                                    auto selectedOpt = m_humanPlayer->selectedPosition();

                                    // אם כבר יש כלי מסומן, לחיצה כפולה עליו תבצע קפיצה על המקום במנוע המשחק
                                    if (selectedOpt.has_value())
                                    {
                                        auto pos = *selectedOpt;
                                        auto moveResult = m_gameEngine->requestMove(pos, pos); // שליחת בקשת קפיצה (from == to)

                                        if (moveResult.isAccepted)
                                        {
                                            // מציאת סוג הכלי לטובת רישום המהלך בהיסטוריית המהלכים
                                            kungfu::PieceType pieceType = kungfu::PieceType::Pawn;
                                            auto pieceOpt = m_gameEngine->getBoard()->pieceAt(pos);
                                            if (pieceOpt.has_value() && pieceOpt.value())
                                            {
                                                pieceType = pieceOpt.value()->type();
                                            }
                                            else
                                            {
                                                auto transitOpt = m_gameEngine->getArbiter().getPieceInTransitAt(pos);
                                                if (transitOpt.has_value() && transitOpt.value())
                                                {
                                                    pieceType = transitOpt.value()->type();
                                                }
                                            }

                                            std::string logText = getMoveNotationString(pieceType, BoardPos{pos.row(), pos.col()}, BoardPos{pos.row(), pos.col()});

                                            // זיהוי צבע הכלי שקפץ לצורך רישום בלוג המתאים
                                            auto pieceColor = kungfu::PlayerColor::White;
                                            auto transitOpt = m_gameEngine->getArbiter().getPieceInTransitAt(pos);
                                            if (transitOpt.has_value() && transitOpt.value())
                                            {
                                                pieceColor = transitOpt.value()->color();
                                            }

                                            if (pieceColor == kungfu::PlayerColor::White)
                                            {
                                                m_whiteHistory.push_back(logText);
                                                if (m_whiteHistory.size() > 8)
                                                    m_whiteHistory.erase(m_whiteHistory.begin());
                                            }
                                            else
                                            {
                                                m_blackHistory.push_back(logText);
                                                if (m_blackHistory.size() > 8)
                                                    m_blackHistory.erase(m_blackHistory.begin());
                                            }
                                        }

                                        m_humanPlayer->clearSelection(); // ביטול סימון הכלי לאחר הקפיצה
                                    }
                                    else
                                    {
                                        // אם אין כלי מסומן, נבצע לחיצה רגילה לצורך סימון ראשוני
                                        int virtualX = col * 100 + 50;
                                        int virtualY = row * 100 + 50;
                                        m_humanPlayer->handleClick(virtualX, virtualY);
                                    }

                                    m_isHovering = false;
                                    m_selectedPieceAnim.isJumping = false;
                                    m_selectedPieceAnim.jumpTimer = 0.0f;
                                }
                                else
                                {
                                    m_lastClickTime = m_totalTime;
                                    m_lastClickedTile = clickedTile;

                                    int virtualX = col * 100 + 50;
                                    int virtualY = row * 100 + 50;

                                    auto activeColor = m_gameEngine->currentTurn();
                                    auto selectedBefore = m_humanPlayer->selectedPosition();

                                    kungfu::PieceType movingPieceType = kungfu::PieceType::Pawn;
                                    if (selectedBefore.has_value())
                                    {
                                        auto board = m_gameEngine->getBoard();
                                        if (board)
                                        {
                                            auto pieceOpt = board->pieceAt(selectedBefore.value());
                                            if (pieceOpt.has_value() && pieceOpt.value())
                                            {
                                                movingPieceType = pieceOpt.value()->type();
                                                activeColor = pieceOpt.value()->color();
                                            }
                                            else
                                            {
                                                auto transitOpt = m_gameEngine->getArbiter().getPieceInTransitAt(selectedBefore.value());
                                                if (transitOpt.has_value() && transitOpt.value())
                                                {
                                                    movingPieceType = transitOpt.value()->type();
                                                    activeColor = transitOpt.value()->color();
                                                }
                                            }
                                        }
                                    }

                                    auto result = m_humanPlayer->handleClick(virtualX, virtualY);
                                    auto selectedAfter = m_humanPlayer->selectedPosition();

                                    if (!selectedBefore.has_value() && selectedAfter.has_value())
                                    {
                                        m_selectedPieceAnim.isJumping = true;
                                        m_isHovering = false;
                                        m_selectedPieceAnim.jumpTimer = 0.0f;
                                    }
                                    else if (selectedBefore.has_value() && !selectedAfter.has_value())
                                    {
                                        m_selectedPieceAnim.isJumping = false;
                                        m_isHovering = false;
                                        m_selectedPieceAnim.jumpTimer = 0.0f;
                                    }

                                    if (result.actionTaken && result.from.has_value() && result.to.has_value() && selectedBefore.has_value())
                                    {
                                        auto snapshot = kungfu::view::SnapshotBuilder::build(
                                            *m_gameEngine->getBoard(),
                                            m_gameEngine->getArbiter(),
                                            m_gameEngine->getCurrentTimeMs(),
                                            m_gameEngine->isGameOver(),
                                            selectedBefore,
                                            m_boardRangeX / 8);

                                        auto requests = m_humanPlayer->decideActions(snapshot); // ניקוז הבקשה הממתינות כדי שלא תצטבר

                                        // אנו בודקים האם המהלך התקבל בהצלחה ישירות מתוצאת הקליק המקורית
                                        bool moveAccepted = (result.description.find("Move requested:") == 0);

                                        if (moveAccepted)
                                        {
                                            BoardPos fromPos{selectedBefore->row(), selectedBefore->col()};
                                            BoardPos toPos{row, col};
                                            std::string logText = getMoveNotationString(movingPieceType, fromPos, toPos);

                                            if (activeColor == kungfu::PlayerColor::White)
                                            {
                                                m_whiteHistory.push_back(logText);
                                                if (m_whiteHistory.size() > 8)
                                                {
                                                    m_whiteHistory.erase(m_whiteHistory.begin());
                                                }
                                            }
                                            else
                                            {
                                                m_blackHistory.push_back(logText);
                                                if (m_blackHistory.size() > 8)
                                                {
                                                    m_blackHistory.erase(m_blackHistory.begin());
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    else
                    {
                        m_hoveredTile = BoardPos{-1, -1};
                    }
                }
            }
            else if (event.type == InputEvent::Type::Keyboard)
            {
                if (event.key.key == Key::Escape)
                {
                    if (m_gameEngine->isGameOver())
                    {
                        m_screenManager.popScreen();
                    }
                    else
                    {
                        togglePause();
                    }
                }
                else if (event.key.key == Key::Space)
                {
                    if (!m_gameEngine->isGameOver())
                    {
                        togglePause();
                    }
                }
            }
        }
    }
};