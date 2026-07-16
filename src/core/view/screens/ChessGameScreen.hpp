#pragma once

#include "ui/framework/BaseScreen.hpp"
#include "ui/framework/ScreenManager.hpp"
#include "ui/framework/ISoundPlayer.hpp"
#include "ui/components/CooldownBar.hpp"
#include "ui/components/Button.hpp"
#include "core/engine/GameEngine.hpp"
#include "core/engine/IGameObserver.hpp"
#include "core/input/Controller.hpp"
#include "core/view/SnapshotBuilder.hpp"
#include "core/io/BoardParser.hpp"
#include "core/common/PieceTokenCodec.hpp"
#include <memory>
#include <iostream>
#include <unordered_map>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdlib> // לטובת rand()

class ChessGameScreen : public BaseScreen
{
private:
    struct Particle {
        Vector2D position;
        Vector2D velocity;
        Color color;
        float life = 0.0f;
        float maxLife = 0.0f;
        float size = 0.0f;
    };

    std::shared_ptr<kungfu::GameEngine> m_gameEngine;
    std::shared_ptr<kungfu::Controller> m_controller;
    CooldownBar m_cooldownBar;
    kungfu::GameConfig m_config;
    std::shared_ptr<ISoundPlayer> m_soundPlayer;
    bool m_isPaused = false;

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
    Vector2D m_mousePos{0.0f, 0.0f};

    // משתני אנימציה
    std::vector<Particle> m_particles;
    float m_pauseTransitionProgress = 0.0f;

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

    static int positionKey(const kungfu::Position &pos, int cols)
    {
        return pos.row() * cols + pos.col();
    }

    // פונקציית עזר ליצירת מחרוזת ייצוג מהלך תקין
    std::string getMoveNotationString(kungfu::PieceType type, const BoardPos& from, const BoardPos& to) const
    {
        char pieceChar = kungfu::PieceTokenCodec::toChar(type);
        char fileChar = 'a' + to.col;
        char rankChar = '1' + to.row;

        std::string notation = "";
        notation += pieceChar;
        notation += ": ";
        notation += fileChar;
        notation += rankChar;

        if (from == to) {
            notation += " (Jump)";
        }
        return notation;
    }

    void resetGame()
    {
        std::string startBoard =
            "wR wN wB wQ wK wB wN wR\n"
            "wP wP wP wP wP wP wP wP\n"
            ". . . . . . . .\n"
            ". . . . . . . .\n"
            ". . . . . . . .\n"
            ". . . . . . . .\n"
            "bP bP bP bP bP bP bP bP\n"
            "bR bN bB bQ bK bB bN bR\n";

        auto board = kungfu::BoardParser::parse(startBoard);
        auto ruleEngine = std::make_shared<kungfu::RuleEngine>(board);

        m_gameEngine = std::make_shared<kungfu::GameEngine>(board, ruleEngine, m_config);
        m_controller = std::make_shared<kungfu::Controller>(m_gameEngine);

        m_controller->setCellSize(100);

        struct GameEventObserver : public kungfu::IGameObserver
        {
            std::shared_ptr<ISoundPlayer> player;
            ChessGameScreen* screen;
            
            GameEventObserver(std::shared_ptr<ISoundPlayer> p, ChessGameScreen* s) 
                : player(std::move(p)), screen(s) {}

            void onMoveCompleted(const kungfu::ArrivalEvent& event, int /*currentTimeMs*/) override {
                if (event.cancelled || !event.piece) {
                    return;
                }
                if (event.capturedKing) {
                    player->playSound("game_over");
                } else if (event.isCapture) {
                    player->playSound("capture");
                }

                if (event.isCapture && screen) {
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
        m_particles.clear();

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

    void spawnCaptureExplosion(const kungfu::Position& boardPos, kungfu::PlayerColor attackerColor)
    {
        float cellWidth = m_boardRangeX / 8.0f;
        float cellHeight = m_boardRangeY / 8.0f;
        
        float px = m_boardStartX + boardPos.col() * cellWidth + cellWidth / 2.0f;
        float py = m_boardStartY + boardPos.row() * cellHeight + cellHeight / 2.0f;

        Color targetColor = (attackerColor == kungfu::PlayerColor::White) 
            ? Color{55, 55, 60, 255}
            : Color{245, 245, 240, 255};

        for (int i = 0; i < 35; ++i) {
            Particle p;
            p.position = { px, py };
            
            float angle = (static_cast<float>(std::rand()) / RAND_MAX) * 2.0f * 3.14159f;
            float speed = 70.0f + (static_cast<float>(std::rand()) / RAND_MAX) * 190.0f;

            p.velocity = {
                std::cos(angle) * speed,
                std::sin(angle) * speed - 60.0f
            };

            if (std::rand() % 4 == 0) {
                p.color = Color{240, 190, 70, 255};
            } else if (std::rand() % 5 == 0) {
                p.color = Color{215, 60, 60, 255};
            } else {
                p.color = targetColor;
            }

            p.maxLife = 0.4f + (static_cast<float>(std::rand()) / RAND_MAX) * 0.5f;
            p.life = p.maxLife;
            p.size = 2.5f + (static_cast<float>(std::rand()) / RAND_MAX) * 5.0f;

            m_particles.push_back(p);
        }
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

    void drawBoardBackground(IRenderer &renderer)
    {
        Color boardFrameBg{35, 36, 43, 255};
        Color boardFrameBorder{65, 68, 85, 255};

        renderer.drawRectangle({m_boardStartX - 8.0f, m_boardStartY - 8.0f},
                               {m_boardRangeX + 16.0f, m_boardRangeY + 16.0f},
                               boardFrameBg, true);
        renderer.drawRectangle({m_boardStartX - 8.0f, m_boardStartY - 8.0f},
                               {m_boardRangeX + 16.0f, m_boardRangeY + 16.0f},
                               boardFrameBorder, false);
    }

    void drawBoardGrid(IRenderer &renderer, int rows, int cols, float cellWidth, float cellHeight,
                       const std::optional<kungfu::Position> &selectedOpt)
    {
        Color lightTile{238, 238, 210, 255};
        Color darkTile{118, 150, 86, 255};
        Color hoverBorderColor{0, 229, 255, 200};
        Color selectedTileColor{244, 246, 128, 110};
        Color selectedBorderColor{244, 246, 128, 255};

        for (int r = 0; r < rows; ++r)
        {
            for (int c = 0; c < cols; ++c)
            {
                Vector2D pos{m_boardStartX + c * cellWidth, m_boardStartY + r * cellHeight};
                Vector2D size{cellWidth, cellHeight};

                Color tileColor = ((r + c) % 2 == 0) ? lightTile : darkTile;
                renderer.drawRectangle(pos, size, tileColor, true);

                if (m_hoveredTile.isValid() && m_hoveredTile.row == r && m_hoveredTile.col == c)
                {
                    renderer.drawRectangle({pos.x + 1.0f, pos.y + 1.0f}, {size.x - 2.0f, size.y - 2.0f}, hoverBorderColor, false);
                }
            }
        }

        if (selectedOpt.has_value() && !m_isPaused && !m_gameEngine->isGameOver())
        {
            Vector2D pos{selectedOpt->col() * cellWidth, m_boardStartY + selectedOpt->row() * cellHeight};
            Vector2D size{cellWidth, cellHeight};
            renderer.drawRectangle(pos, size, selectedTileColor, true);
            renderer.drawRectangle(pos, size, selectedBorderColor, false);
        }
    }

    void drawPiecesAndCooldowns(IRenderer &renderer, const kungfu::view::GameSnapshot &snapshot,
                                float cellWidth, float cellHeight,
                                const std::optional<kungfu::Position> &selectedOpt, int cols,
                                std::unordered_map<int, Vector2D> &outPiecePositions)
    {
        for (const auto &pieceSnap : snapshot.pieces)
        {
            std::string assetId = (pieceSnap.color == kungfu::PlayerColor::White) ? "w" : "b";
            assetId += kungfu::PieceTokenCodec::toChar(pieceSnap.type);

            float animatedX = pieceSnap.pixelX;
            float animatedY = m_boardStartY + pieceSnap.pixelY;

            if (selectedOpt.has_value() &&
                pieceSnap.logicalPosition.row() == selectedOpt->row() &&
                pieceSnap.logicalPosition.col() == selectedOpt->col())
            {
                if (m_isHovering)
                {
                    float floatOffset = 25.0f + std::sin(m_selectedPieceAnim.jumpTimer * 5.0f) * 6.0f;
                    animatedY -= floatOffset;
                }
                else if (m_selectedPieceAnim.isJumping)
                {
                    float bounceOffset = std::abs(std::sin(m_selectedPieceAnim.jumpTimer * 12.0f)) * 22.0f;
                    animatedY -= bounceOffset;

                    if (m_selectedPieceAnim.jumpTimer * 12.0f >= 3.14159f)
                    {
                        m_selectedPieceAnim.isJumping = false;
                    }
                }
            }

            Vector2D spritePos{animatedX, animatedY};
            Vector2D spriteSize{cellWidth, cellHeight};

            float padding = cellWidth * 0.10f;
            renderer.drawSprite(assetId, {spritePos.x + padding, spritePos.y + padding}, {spriteSize.x - (padding * 2), spriteSize.y - (padding * 2)});

            // הצגת הצינון רק אם תנועה סימולטנית (זמן אמת) מאופשרת
            if (m_config.allowSimultaneousMovement && pieceSnap.cooldownProgress > 0.0f)
            {
                Vector2D center{animatedX + cellWidth / 2.0f, animatedY + cellHeight / 2.0f};
                float radius = cellWidth * 0.42f;

                float startAngle = -90.0f;
                float endAngle = -90.0f + (360.0f * pieceSnap.cooldownProgress);

                Color radialColor{180, 35, 35, 110};
                renderer.drawSector(center, radius, startAngle, endAngle, radialColor, true);

                renderer.drawCircle(center, radius, {220, 50, 50, 90}, false);

                m_cooldownBar.draw(renderer, center, pieceSnap.cooldownProgress);
            }

            outPiecePositions[positionKey(pieceSnap.logicalPosition, cols)] = spritePos;
        }
    }

    void drawPremoves(IRenderer &renderer, float cellWidth, float cellHeight, int cols,
                      const std::unordered_map<int, Vector2D> &piecePositions)
    {
        if (m_isPaused || m_gameEngine->isGameOver())
        {
            return;
        }

        const auto &premoveQueue = m_gameEngine->getPremoveQueue();
        for (const auto &entry : premoveQueue.entries())
        {
            auto piece = entry.first;
            auto to = entry.second;
            if (!piece)
                continue;

            float destX = to.col() * cellWidth;
            float destY = m_boardStartY + to.row() * cellHeight;

            Color premoveColor{255, 107, 107, 70};
            Color premoveBorderColor{255, 107, 107, 200};

            renderer.drawRectangle({destX, destY}, {cellWidth, cellHeight}, premoveColor, true);
            renderer.drawRectangle({destX, destY}, {cellWidth, cellHeight}, premoveBorderColor, false);

            Vector2D startPt{0.0f, 0.0f};
            auto it = piecePositions.find(positionKey(piece->position(), cols));
            if (it != piecePositions.end())
            {
                startPt.x = it->second.x + cellWidth / 2.0f;
                startPt.y = it->second.y + cellHeight / 2.0f;
            }
            else
            {
                startPt.x = piece->position().col() * cellWidth + cellWidth / 2.0f;
                startPt.y = m_boardStartY + piece->position().row() * cellHeight + cellHeight / 2.0f;
            }

            Vector2D endPt{destX + cellWidth / 2.0f, destY + cellHeight / 2.0f};
            renderer.drawLine(startPt, endPt, premoveBorderColor, 3.0f);
        }
    }

    void drawHeader(IRenderer &renderer)
    {
        renderer.drawRectangle({0.0f, 0.0f}, {1000.0f, 95.0f}, m_theme.background, true);
        renderer.drawLine({0.0f, 95.0f}, {1000.0f, 95.0f}, m_theme.border, 2.0f);

        renderer.drawText("KUNG-FU CHESS", {30.0f, 60.0f}, 24, m_theme.titleText);

        m_pauseButton->draw(renderer);
        m_sidebarRestartButton->draw(renderer);
        m_sidebarMenuButton->draw(renderer);
    }

    void drawSidebar(IRenderer &renderer)
    {
        renderer.drawRectangle({800.0f, 100.0f}, {200.0f, 800.0f}, m_theme.background, true);
        renderer.drawLine({800.0f, 100.0f}, {800.0f, 900.0f}, m_theme.border, 2.0f);

        renderer.drawRectangle({810.0f, 110.0f}, {180.0f, 360.0f}, {30, 30, 40, 255}, true);
        renderer.drawRectangle({810.0f, 110.0f}, {180.0f, 360.0f}, m_theme.border, false);
        renderer.drawText("WHITE MOVES", {825.0f, 145.0f}, 13, {255, 255, 255, 255});
        renderer.drawLine({815.0f, 160.0f}, {985.0f, 160.0f}, {50, 50, 65, 255}, 1.0f);

        float whiteLogY = 195.0f;
        for (const auto &logEntry : m_whiteHistory)
        {
            Color textColor = {200, 200, 210, 255};
            if (logEntry.find("rejected") != std::string::npos || logEntry.find("failed") != std::string::npos)
            {
                textColor = {240, 100, 100, 255};
            }
            else if (logEntry.find("ok") != std::string::npos || logEntry.find("Connected") != std::string::npos || logEntry.find(":") != std::string::npos)
            {
                textColor = {100, 210, 130, 255};
            }
            renderer.drawText(logEntry, {825.0f, whiteLogY}, 10, textColor);
            whiteLogY += 35.0f;
        }

        renderer.drawRectangle({810.0f, 490.0f}, {180.0f, 360.0f}, {30, 30, 40, 255}, true);
        renderer.drawRectangle({810.0f, 490.0f}, {180.0f, 360.0f}, m_theme.border, false);
        renderer.drawText("BLACK MOVES", {825.0f, 525.0f}, 13, {160, 160, 175, 255});
        renderer.drawLine({815.0f, 540.0f}, {985.0f, 540.0f}, {50, 50, 65, 255}, 1.0f);

        float blackLogY = 575.0f;
        for (const auto &logEntry : m_blackHistory)
        {
            Color textColor = {170, 170, 185, 255};
            if (logEntry.find("rejected") != std::string::npos || logEntry.find("failed") != std::string::npos)
            {
                textColor = {240, 100, 100, 255};
            }
            else if (logEntry.find("ok") != std::string::npos || logEntry.find("Connected") != std::string::npos || logEntry.find(":") != std::string::npos)
            {
                textColor = {100, 210, 130, 255};
            }
            renderer.drawText(logEntry, {825.0f, blackLogY}, 10, textColor);
            blackLogY += 35.0f;
        }
    }

    void drawFooter(IRenderer &renderer)
    {
        renderer.drawRectangle({0.0f, 905.0f}, {1000.0f, 95.0f}, m_theme.background, true);
        renderer.drawLine({0.0f, 905.0f}, {1000.0f, 905.0f}, m_theme.border, 2.0f);

        int whiteScore = calculatePlayerScore(kungfu::PlayerColor::White);
        int blackScore = calculatePlayerScore(kungfu::PlayerColor::Black);
        renderer.drawText("WHITE Score: " + std::to_string(whiteScore) + " / 39", {40.0f, 960.0f}, 15, {255, 255, 255, 255});
        renderer.drawText("BLACK Score: " + std::to_string(blackScore) + " / 39", {340.0f, 960.0f}, 15, {170, 170, 180, 255});

        renderer.drawText("GAME STATUS: ", {640.0f, 960.0f}, 13, {150, 150, 170, 255});
        if (m_gameEngine->isGameOver())
        {
            renderer.drawText("Game Over!", {750.0f, 960.0f}, 14, {240, 80, 80, 255});
        }
        else if (m_isPaused)
        {
            renderer.drawText("Paused", {750.0f, 960.0f}, 14, {240, 200, 80, 255});
        }
        else
        {
            if (!m_config.allowSimultaneousMovement)
            {
                if (m_gameEngine->currentTurn() == kungfu::PlayerColor::White)
                {
                    renderer.drawText("White's Turn", {750.0f, 960.0f}, 14, {240, 240, 250, 255});
                }
                else
                {
                    renderer.drawText("Black's Turn", {750.0f, 960.0f}, 14, {150, 150, 160, 255});
                }
            }
            else
            {
                renderer.drawText("Real-Time Battle!", {750.0f, 960.0f}, 14, {80, 180, 240, 255});
            }
        }
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

        float cellWidth = m_boardRangeX / cols;
        float cellHeight = m_boardRangeY / rows;

        auto selectedOpt = m_controller->selectedPosition();
        auto snapshot = kungfu::view::SnapshotBuilder::build(
            *board,
            m_gameEngine->getArbiter(),
            m_gameEngine->getCurrentTimeMs(),
            m_gameEngine->isGameOver(),
            selectedOpt,
            cellWidth);

        std::unordered_map<int, Vector2D> piecePositions;

        drawBoardBackground(renderer);
        drawBoardGrid(renderer, rows, cols, cellWidth, cellHeight, selectedOpt);
        drawPiecesAndCooldowns(renderer, snapshot, cellWidth, cellHeight, selectedOpt, cols, piecePositions);
        drawPremoves(renderer, cellWidth, cellHeight, cols, piecePositions);
        
        for (const auto& p : m_particles) {
            Color fadeColor = p.color;
            fadeColor.a = static_cast<std::uint8_t>((p.life / p.maxLife) * 255);
            renderer.drawCircle(p.position, p.size, fadeColor, true);
        }

        drawHeader(renderer);
        drawSidebar(renderer);
        drawFooter(renderer);
        drawOverlays(renderer, snapshot);
    }

public:
    explicit ChessGameScreen(ScreenManager &manager,
                             kungfu::GameConfig config = kungfu::GameConfig{},
                             std::shared_ptr<ISoundPlayer> soundPlayer = std::make_shared<NullSoundPlayer>())
        : BaseScreen(manager, "Chess Match"), m_config(config), m_soundPlayer(std::move(soundPlayer))
    {
        if (!m_config.allowSimultaneousMovement) {
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
        if (!m_config.allowSimultaneousMovement) {
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

        for (auto it = m_particles.begin(); it != m_particles.end(); ) {
            it->life -= deltaTime;
            if (it->life <= 0.0f) {
                it = m_particles.erase(it);
            } else {
                it->position.x += it->velocity.x * deltaTime;
                it->position.y += it->velocity.y * deltaTime;
                it->velocity.y += 120.0f * deltaTime;
                ++it;
            }
        }

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

        auto selectedOpt = m_controller->selectedPosition();
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
                m_mousePos = {mouse.logicalX, mouse.logicalY};

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

                                if (clickedTile == m_lastClickedTile && timeSinceLastClick < 0.48f && timeSinceLastClick > 0.001f)
                                {
                                    m_lastClickTime = 0.0f;
                                    m_lastClickedTile = BoardPos{-1, -1};

                                    auto selectedOpt = m_controller->selectedPosition();

                                    if (!selectedOpt.has_value())
                                    {
                                        int virtualX = col * 100 + 50;
                                        int virtualY = row * 100 + 50;
                                        m_controller->click(virtualX, virtualY);
                                    }

                                    m_isHovering = !m_isHovering;
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
                                    auto selectedBefore = m_controller->selectedPosition();

                                    // איתור סוג הכלי שנבחר לפני הקליק
                                    kungfu::PieceType movingPieceType = kungfu::PieceType::Pawn;
                                    if (selectedBefore.has_value()) {
                                        auto board = m_gameEngine->getBoard();
                                        if (board) {
                                            auto pieceOpt = board->pieceAt(selectedBefore.value());
                                            if (pieceOpt.has_value() && pieceOpt.value()) {
                                                movingPieceType = pieceOpt.value()->type();
                                                activeColor = pieceOpt.value()->color();
                                            } else {
                                                auto transitOpt = m_gameEngine->getArbiter().getPieceInTransitAt(selectedBefore.value());
                                                if (transitOpt.has_value() && transitOpt.value()) {
                                                    movingPieceType = transitOpt.value()->type();
                                                    activeColor = transitOpt.value()->color();
                                                }
                                            }
                                        }
                                    }

                                    auto result = m_controller->click(virtualX, virtualY);
                                    auto selectedAfter = m_controller->selectedPosition();

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

                                    if (result.actionTaken && !result.description.empty())
                                    {
                                        // רישום המהלך בטבלה רק אם הוא אושר ובוצע בהצלחה
                                        bool isMoveAccepted = (result.description.rfind("Move requested: ok", 0) == 0 || 
                                                               result.description.rfind("Move requested: jump_started", 0) == 0);

                                        if (isMoveAccepted && selectedBefore.has_value()) {
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