#include "core/realtime/CollisionDetector.hpp"
#include <algorithm>

namespace kungfu
{
    namespace
    {

        // בדיקת חפיפת זמנים פשוטה
        bool hasTimeOverlap(const kungfu::Motion &m1, const kungfu::Motion &m2) noexcept
        {
            int maxStart = std::max(m1.startTime(), m2.startTime());
            int minArrival = std::min(m1.arrivalTime(), m2.arrivalTime());
            return maxStart < minArrival;
        }

        // בדיקת חפיפת קופסה חוסמת (Bounding Box) במרחב הלוח
        bool hasBoundingBoxOverlap(const kungfu::Motion &m1, const kungfu::Motion &m2) noexcept
        {
            int minRow1 = std::min(m1.from().row(), m1.to().row());
            int maxRow1 = std::max(m1.from().row(), m1.to().row());
            int minCol1 = std::min(m1.from().col(), m1.to().col());
            int maxCol1 = std::max(m1.from().col(), m1.to().col());

            int minRow2 = std::min(m2.from().row(), m2.to().row());
            int maxRow2 = std::max(m2.from().row(), m2.to().row());
            int minCol2 = std::min(m2.from().col(), m2.to().col());
            int maxCol2 = std::max(m2.from().col(), m2.to().col());

            return !(maxRow1 < minRow2 || minRow1 > maxRow2 || maxCol1 < minCol2 || minCol1 > maxCol2);
        }

    } // namespace

    // פונקציית עזר שמחזירה את רשימת המשבצות שבהן הכלי עובר (כולל start ו-end)
    std::vector<Position> getDetailedPath(const Position &from, const Position &to)
    {
        std::vector<Position> path;
        path.push_back(from);

        int dr = to.row() - from.row();
        int dc = to.col() - from.col();
        if (dr == 0 && dc == 0)
            return path;

        int stepR = (dr > 0) ? 1 : ((dr < 0) ? -1 : 0);
        int stepC = (dc > 0) ? 1 : ((dc < 0) ? -1 : 0);

        int curR = from.row() + stepR;
        int curC = from.col() + stepC;

        while (curR != to.row() || curC != to.col())
        {
            path.emplace_back(curR, curC);
            curR += stepR;
            curC += stepC;
        }
        path.push_back(to);
        return path;
    }

    std::vector<MidRouteCollision> CollisionDetector::detectMidRouteCollisions(
        const std::vector<Motion> &activeMotions,
        const GameConfig &config) noexcept
    {
        std::vector<MidRouteCollision> collisions;
        if (!config.allowSimultaneousMovement || activeMotions.size() < 2)
        {
            return collisions;
        }

        for (size_t i = 0; i < activeMotions.size(); ++i)
        {
            for (size_t j = i + 1; j < activeMotions.size(); ++j)
            {
                const auto &m1 = activeMotions[i];
                const auto &m2 = activeMotions[j];

                // 1. סינון לפי סוג כלי (פרשים) ולפי מצב (Airborne = "לא שם")
                if (m1.piece()->type() == PieceType::Knight || m2.piece()->type() == PieceType::Knight)
                {
                    continue;
                }
                if (m1.piece()->state() == PieceState::Airborne || m2.piece()->state() == PieceState::Airborne)
                {
                    continue;
                }

                // 2. סינון מהיר (Broad Phase): בדיקת חפיפת זמנים כוללת
                if (!hasTimeOverlap(m1, m2))
                {
                    continue;
                }

                // 3. סינון מהיר (Broad Phase): בדיקת קופסה חוסמת מרחבית
                if (!hasBoundingBoxOverlap(m1, m2))
                {
                    continue;
                }

                // 4. בדיקה מעמיקה (Narrow Phase) רק עבור זוגות רלוונטיים
                if (auto collision = checkDetailedCollision(m1, m2))
                {
                    collisions.push_back(*collision);
                }
            }
        }
        return collisions;
    }

    std::optional<kungfu::MidRouteCollision> CollisionDetector::checkDetailedCollision(
        const Motion &m1, const Motion &m2) noexcept
    {
        auto path1 = getDetailedPath(m1.from(), m1.to());
        auto path2 = getDetailedPath(m2.from(), m2.to());

        int duration1 = m1.arrivalTime() - m1.startTime();
        int duration2 = m2.arrivalTime() - m2.startTime();

        float timePerStep1 = (path1.size() > 1) ? (float)duration1 / (path1.size() - 1) : 0;
        float timePerStep2 = (path2.size() > 1) ? (float)duration2 / (path2.size() - 1) : 0;

        for (size_t idx1 = 0; idx1 < path1.size(); ++idx1)
        {
            for (size_t idx2 = 0; idx2 < path2.size(); ++idx2)
            {
                if (path1[idx1] == path2[idx2])
                {

                    int t1_enter = m1.startTime() + static_cast<int>(idx1 * timePerStep1);
                    int t2_enter = m2.startTime() + static_cast<int>(idx2 * timePerStep2);

                    int t1_exit = (idx1 == path1.size() - 1) ? m1.arrivalTime() + 100000 : m1.startTime() + static_cast<int>((idx1 + 1) * timePerStep1);
                    int t2_exit = (idx2 == path2.size() - 1) ? m2.arrivalTime() + 100000 : m2.startTime() + static_cast<int>((idx2 + 1) * timePerStep2);

                  
                    if (std::max(t1_enter, t2_enter) < std::min(t1_exit, t2_exit))
                    {
                        // 1. מי שיצא לדרך מוקדם יותר (startTime נמוך יותר) הוא המנצח
                        if (m1.startTime() < m2.startTime())
                        {
                            return MidRouteCollision{m1, m2}; // m1 הוא winner, m2 הוא loser
                        }
                        else if (m2.startTime() < m1.startTime())
                        {
                            return MidRouteCollision{m2, m1}; // m2 הוא winner, m1 הוא loser
                        }
                        else
                        {
                            // 2. שובר שוויון: אם הם יצאו בדיוק באותו זמן, מי שהגיע פיזית ראשון למשבצת המפגש מנצח
                            if (t1_enter <= t2_enter)
                            {
                                return MidRouteCollision{m1, m2};
                            }
                            else
                            {
                                return MidRouteCollision{m2, m1};
                            }
                        }
                    }
                }
            }
        }
        return std::nullopt;
    }
    std::vector<Motion> CollisionDetector::detectArrivals(
        const std::vector<Motion> &activeMotions,
        int currentTimeMs) noexcept
    {
        std::vector<Motion> arrivals;
        for (const auto &m : activeMotions)
        {
            if (currentTimeMs >= m.arrivalTime())
            {
                arrivals.push_back(m);
            }
        }
        return arrivals;
    }

} // namespace kungfu