// ui/components/GameOverlaysView.hpp
#pragma once

#include "ui/framework/IRenderer.hpp"
#include "ui/components/Button.hpp"
#include "players/network/NetworkPlayer.hpp"
#include "engine/snapshot/GameSnapshot.hpp"
#include <memory>
#include <string>
#include <optional>
#include <cmath>
#include <algorithm>

class GameOverlaysView {
public:
    GameOverlaysView() = default;

    void drawMatchmaking(IRenderer& renderer, float searchTimer, float totalTime,
                        std::uint64_t roomCode, Button& cancelButton,
                        float boardX, float boardY, float boardW, float boardH) const 
    {
        renderer.drawRectangle({boardX, boardY}, {boardW, boardH}, {12, 13, 18, 235}, true);

        Vector2D panelPos{150.0f, 280.0f};
        Vector2D panelSize{500.0f, 320.0f};

        renderer.drawRectangle(panelPos, panelSize, {25, 28, 38, 250}, true);
        renderer.drawRectangle(panelPos, panelSize, {48, 120, 192, 255}, false);

        if (roomCode != 0) {
            renderer.drawText("PRIVATE ROOM #" + std::to_string(roomCode), {panelPos.x + 120.0f, panelPos.y + 60.0f}, 24, {240, 200, 80, 255});
            renderer.drawText("Waiting for opponent to enter this code...", {panelPos.x + 90.0f, panelPos.y + 110.0f}, 14, {210, 215, 225, 255});
            renderer.drawText("Share code #" + std::to_string(roomCode) + " with your friend!", {panelPos.x + 110.0f, panelPos.y + 150.0f}, 13, {160, 165, 180, 255});
        } else {
            renderer.drawText("MATCHMAKING", {panelPos.x + 160.0f, panelPos.y + 60.0f}, 28, {240, 200, 80, 255});
            renderer.drawText("Searching for opponent...", {panelPos.x + 150.0f, panelPos.y + 110.0f}, 15, {210, 215, 225, 255});

            float pulse = (std::sin(totalTime * 4.0f) + 1.0f) * 0.5f;
            renderer.drawCircle({panelPos.x + 250.0f, panelPos.y + 155.0f}, 15.0f + pulse * 8.0f, {48, 120, 192, static_cast<std::uint8_t>(100 + pulse * 155)}, true);
        }

        int sec = static_cast<int>(searchTimer);
        renderer.drawText("Time in Queue: " + std::to_string(sec) + "s", {panelPos.x + 175.0f, panelPos.y + 200.0f}, 14, {180, 185, 200, 255});

        cancelButton.draw(renderer);
    }

    void drawMatchFoundBanner(IRenderer& renderer, float bannerTimer,
                              const std::shared_ptr<kungfu::NetworkPlayer>& netPlayer) const 
    {
        float alphaFactor = std::min(bannerTimer, 1.0f);
        renderer.drawRectangle({100.0f, 120.0f}, {600.0f, 90.0f}, {20, 25, 35, static_cast<std::uint8_t>(alphaFactor * 240)}, true);
        renderer.drawRectangle({100.0f, 120.0f}, {600.0f, 90.0f}, {74, 222, 128, static_cast<std::uint8_t>(alphaFactor * 255)}, false);

        std::string myColorStr = (netPlayer && netPlayer->assignedColor() == kungfu::PlayerColor::White) ? "WHITE" : "BLACK";
        std::string oppInfo = netPlayer ? (netPlayer->opponentUsername() + " (" + std::to_string(netPlayer->opponentRating()) + ")") : "Opponent";

        renderer.drawText("MATCH FOUND!", {320.0f, 150.0f}, 20, {74, 222, 128, static_cast<std::uint8_t>(alphaFactor * 255)});
        renderer.drawText("You play as " + myColorStr + " vs " + oppInfo, {180.0f, 185.0f}, 14, {255, 255, 255, static_cast<std::uint8_t>(alphaFactor * 255)});
    }

    void drawPauseOverlay(IRenderer& renderer, float transitionProgress, float boardX, float boardY, float boardW, float boardH) const 
    {
        renderer.drawRectangle({boardX, boardY}, {boardW, boardH}, {15, 15, 20, static_cast<std::uint8_t>(transitionProgress * 180)}, true);
        float panelY = -200.0f + 500.0f * (transitionProgress * transitionProgress * (3.0f - 2.0f * transitionProgress));
        renderer.drawRectangle({350.0f, panelY}, {300.0f, 150.0f}, {25, 25, 35, static_cast<std::uint8_t>(transitionProgress * 240)}, true);
        renderer.drawRectangle({350.0f, panelY}, {300.0f, 150.0f}, {80, 80, 100, static_cast<std::uint8_t>(transitionProgress * 255)}, false);
        renderer.drawText("PAUSED", {445.0f, panelY + 50.0f}, 38, {240, 200, 80, static_cast<std::uint8_t>(transitionProgress * 255)});
        renderer.drawText("Press SPACE or Resume", {390.0f, panelY + 110.0f}, 14, {180, 180, 190, static_cast<std::uint8_t>(transitionProgress * 255)});
    }

    void drawDisconnectCountdown(IRenderer& renderer, int secondsLeft) const 
    {
        renderer.drawRectangle({150.0f, 350.0f}, {500.0f, 150.0f}, {20, 20, 25, 230}, true);
        renderer.drawRectangle({150.0f, 350.0f}, {500.0f, 150.0f}, {220, 60, 60, 255}, false);
        renderer.drawText("OPPONENT DISCONNECTED", {230.0f, 400.0f}, 24, {220, 60, 60, 255});
        renderer.drawText("Auto-Resign in: " + std::to_string(secondsLeft) + " seconds", {280.0f, 450.0f}, 16, {255, 255, 255, 255});
    }

    void drawGameOverOverlay(IRenderer& renderer,
                             std::optional<kungfu::PlayerColor> winnerOpt,
                             bool isNetworkMode,
                             const std::shared_ptr<kungfu::NetworkPlayer>& netPlayer,
                             bool isAiOpponent,
                             Button& rematchButton,
                             Button& menuButton,
                             float boardX, float boardY, float boardW, float boardH) const 
    {
        renderer.drawRectangle({boardX, boardY}, {boardW, boardH}, {10, 10, 15, 160}, true);

        bool drawVictoryText = false;
        bool drawDefeatText = false;
        std::string neutralResultText = "MATCH ENDED";
        std::string subTitleText = "An epic real-time match!";

        if (winnerOpt.has_value()) {
            if (isNetworkMode && netPlayer) {
                if (winnerOpt.value() == netPlayer->assignedColor()) drawVictoryText = true;
                else drawDefeatText = true;
            } else if (isAiOpponent) {
                if (winnerOpt.value() == kungfu::PlayerColor::White) drawVictoryText = true;
                else drawDefeatText = true;
            } else {
                neutralResultText = (winnerOpt.value() == kungfu::PlayerColor::White) ? "WHITE WINS!" : "BLACK WINS!";
            }
        } else {
            if (isNetworkMode && netPlayer && (netPlayer->matchEnded() || netPlayer->opponentDisconnected())) {
                drawVictoryText = true;
                subTitleText = "Opponent forfeited / timed out!";
            } else {
                neutralResultText = "IT'S A DRAW!";
            }
        }

        Vector2D panelPos{150.0f, 320.0f};
        Vector2D panelSize{500.0f, 320.0f};

        renderer.drawRectangle({panelPos.x + 8.0f, panelPos.y + 8.0f}, panelSize, {0, 0, 0, 120}, true);

        if (drawVictoryText) {
            renderer.drawRectangle(panelPos, panelSize, {30, 35, 45, 245}, true);
            renderer.drawRectangle(panelPos, panelSize, {240, 200, 80, 255}, false);
            renderer.drawText("VICTORY!", {panelPos.x + 130.0f, panelPos.y + 110.0f}, 52, {240, 200, 80, 255});
            renderer.drawText(subTitleText, {panelPos.x + 135.0f, panelPos.y + 160.0f}, 14, {180, 185, 200, 255});
        } else if (drawDefeatText) {
            renderer.drawRectangle(panelPos, panelSize, {25, 20, 25, 245}, true);
            renderer.drawRectangle(panelPos, panelSize, {210, 60, 60, 255}, false);
            renderer.drawText("DEFEAT", {panelPos.x + 155.0f, panelPos.y + 110.0f}, 52, {210, 60, 60, 255});
            renderer.drawText("Defeat is a step to mastery.", {panelPos.x + 145.0f, panelPos.y + 160.0f}, 14, {160, 160, 170, 255});
        } else {
            renderer.drawRectangle(panelPos, panelSize, {25, 27, 35, 245}, true);
            renderer.drawRectangle(panelPos, panelSize, {100, 105, 120, 255}, false);
            renderer.drawText(neutralResultText, {panelPos.x + 110.0f, panelPos.y + 110.0f}, 42, {220, 225, 235, 255});
            renderer.drawText("Good game!", {panelPos.x + 205.0f, panelPos.y + 160.0f}, 14, {160, 165, 175, 255});
        }

        rematchButton.draw(renderer);
        menuButton.draw(renderer);
    }
};