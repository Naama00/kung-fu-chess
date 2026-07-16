#pragma once

#include "ui/framework/IRenderer.hpp"
#include "ui/framework/InputEvents.hpp"
#include "engine/snapshot/GameSnapshot.hpp"
#include "engine/core/PremoveQueue.hpp"
#include "engine/common/PieceTokenCodec.hpp"
#include "ui/components/CooldownBar.hpp"
#include <unordered_map>
#include <optional>
#include <cmath>

class BoardView {
private:
    CooldownBar m_cooldownBar; // מד הצינון מנוהל כעת פנימית ברכיב הלוח

    static int positionKey(const kungfu::Position &pos, int cols) {
        return pos.row() * cols + pos.col();
    }

public:
    BoardView() = default;
    ~BoardView() = default;

    BoardView(const BoardView&) = delete;
    BoardView& operator=(const BoardView&) = delete;

    /**
     * פונקציית הציור הראשית של לוח המשחק.
     */
    void draw(IRenderer& renderer,
              const kungfu::view::GameSnapshot& snapshot,
              const kungfu::PremoveQueue& premoveQueue,
              float boardStartX,
              float boardStartY,
              float boardRangeX,
              float boardRangeY,
              int hoveredRow,
              int hoveredCol,
              bool isHovering,
              bool isJumping,
              float jumpTimer,
              bool isPaused,
              bool allowSimultaneous) const
    {
        int rows = snapshot.boardRows;
        int cols = snapshot.boardCols;

        float cellWidth = boardRangeX / cols;
        float cellHeight = boardRangeY / rows;

        // --- 1. ציור רקע הלוח והמסגרות הקישוטיות ---
        Color boardFrameBg{35, 36, 43, 255};
        Color boardFrameBorder{65, 68, 85, 255};

        renderer.drawRectangle({boardStartX - 8.0f, boardStartY - 8.0f},
                               {boardRangeX + 16.0f, boardRangeY + 16.0f},
                               boardFrameBg, true);
        renderer.drawRectangle({boardStartX - 8.0f, boardStartY - 8.0f},
                               {boardRangeX + 16.0f, boardRangeY + 16.0f},
                               boardFrameBorder, false);

        // --- 2. ציור הרשת (Grid), בחירה (Selection) וריחוף (Hover) ---
        Color lightTile{238, 238, 210, 255};
        Color darkTile{118, 150, 86, 255};
        Color hoverBorderColor{0, 229, 255, 200};
        Color selectedTileColor{244, 246, 128, 110};
        Color selectedBorderColor{244, 246, 128, 255};

        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                Vector2D pos{boardStartX + c * cellWidth, boardStartY + r * cellHeight};
                Vector2D size{cellWidth, cellHeight};

                Color tileColor = ((r + c) % 2 == 0) ? lightTile : darkTile;
                renderer.drawRectangle(pos, size, tileColor, true);

                // סימון ריחוף עכבר מעל משבצת
                if (hoveredRow == r && hoveredCol == c) {
                    renderer.drawRectangle({pos.x + 1.0f, pos.y + 1.0f}, {size.x - 2.0f, size.y - 2.0f}, hoverBorderColor, false);
                }
            }
        }

        // סימון משבצת שנבחרה
        auto selectedOpt = snapshot.selectedCell;
        if (selectedOpt.has_value() && !isPaused && !snapshot.isGameOver) {
            Vector2D pos{selectedOpt->col() * cellWidth, boardStartY + selectedOpt->row() * cellHeight};
            Vector2D size{cellWidth, cellHeight};
            renderer.drawRectangle(pos, size, selectedTileColor, true);
            renderer.drawRectangle(pos, size, selectedBorderColor, false);
        }

        // --- 3. ציור הכלים, אפקטי קפיצה וצינונים (Cooldowns) ---
        std::unordered_map<int, Vector2D> piecePositions; // מעקב אחר המיקום הפיזי של הכלים לצורך ציור חיצי ה-Premoves

        for (const auto &pieceSnap : snapshot.pieces) {
            std::string assetId = (pieceSnap.color == kungfu::PlayerColor::White) ? "w" : "b";
            assetId += kungfu::PieceTokenCodec::toChar(pieceSnap.type);

            float animatedX = pieceSnap.pixelX;
            float animatedY = boardStartY + pieceSnap.pixelY;

            // אנימציות ריחוף וקפיצה של כלי מסומן
            if (selectedOpt.has_value() &&
                pieceSnap.logicalPosition.row() == selectedOpt->row() &&
                pieceSnap.logicalPosition.col() == selectedOpt->col())
            {
                if (isHovering) {
                    float floatOffset = 25.0f + std::sin(jumpTimer * 5.0f) * 6.0f;
                    animatedY -= floatOffset;
                } else if (isJumping) {
                    float bounceOffset = std::abs(std::sin(jumpTimer * 12.0f)) * 22.0f;
                    animatedY -= bounceOffset;
                }
            }

            Vector2D spritePos{animatedX, animatedY};
            Vector2D spriteSize{cellWidth, cellHeight};

            float padding = cellWidth * 0.10f;
            renderer.drawSprite(assetId, {spritePos.x + padding, spritePos.y + padding}, {spriteSize.x - (padding * 2), spriteSize.y - (padding * 2)});

            // רינדור אפקטים חזותיים של צינון
            if (allowSimultaneous && pieceSnap.cooldownProgress > 0.0f) {
                Vector2D center{animatedX + cellWidth / 2.0f, animatedY + cellHeight / 2.0f};
                float radius = cellWidth * 0.42f;

                float startAngle = -90.0f;
                float endAngle = -90.0f + (360.0f * pieceSnap.cooldownProgress);

                Color radialColor{180, 35, 35, 110};
                renderer.drawSector(center, radius, startAngle, endAngle, radialColor, true);
                renderer.drawCircle(center, radius, {220, 50, 50, 90}, false);

                m_cooldownBar.draw(renderer, center, pieceSnap.cooldownProgress);
            }

            piecePositions[positionKey(pieceSnap.logicalPosition, cols)] = spritePos;
        }

        // --- 4. ציור מהלכי Premoves וחיבור קווים מנחים ---
        if (!isPaused && !snapshot.isGameOver) {
            for (const auto &entry : premoveQueue.entries()) {
                auto piece = entry.first;
                auto to = entry.second;
                if (!piece) continue;

                float destX = to.col() * cellWidth;
                float destY = boardStartY + to.row() * cellHeight;

                Color premoveColor{255, 107, 107, 70};
                Color premoveBorderColor{255, 107, 107, 200};

                renderer.drawRectangle({destX, destY}, {cellWidth, cellHeight}, premoveColor, true);
                renderer.drawRectangle({destX, destY}, {cellWidth, cellHeight}, premoveBorderColor, false);

                Vector2D startPt{0.0f, 0.0f};
                auto it = piecePositions.find(positionKey(piece->position(), cols));
                if (it != piecePositions.end()) {
                    startPt.x = it->second.x + cellWidth / 2.0f;
                    startPt.y = it->second.y + cellHeight / 2.0f;
                } else {
                    startPt.x = piece->position().col() * cellWidth + cellWidth / 2.0f;
                    startPt.y = boardStartY + piece->position().row() * cellHeight + cellHeight / 2.0f;
                }

                Vector2D endPt{destX + cellWidth / 2.0f, destY + cellHeight / 2.0f};
                renderer.drawLine(startPt, endPt, premoveBorderColor, 3.0f);
            }
        }
    }
};