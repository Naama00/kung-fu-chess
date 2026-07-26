# Real-Time Mechanics, Cooldowns, and Collisions

This document outlines the unique physics and real-time rules implemented in the project. These mechanics are enforced by the `RealTimeArbiter` in coordination with the `CollisionDetector` and `CollisionResolver`.

---

## 1. Continuous Motion
* **Movement Speed:** Pieces traverse the board continuously over time rather than instantly teleporting to their destination. The total movement duration is calculated based on path distance multiplied by the movement speed (`msPerCellSpeed`).
* **Interpolation:** A piece's spatial position is updated continuously using time-based calculation (`interpolatePosition`), allowing the UI layer to render smooth movement animations.

---

## 2. Cooldown Timing
* **Cooldown Tracker:** After executing a move, the piece enters a mandatory cooldown period (`cooldownDurationMs`).
* **Busy Piece State:** Any piece currently moving or undergoing cooldown is flagged as busy and cannot accept new direct movement commands (unless queued as a premove).

---

## 3. Jump in Place (Airborne State)
* **Activation:** Triggered by issuing a movement request targeting the piece's current position (`from == to`).
* **Temporary Immunity:** The piece transitions to the `PieceState::Airborne` state and is logically lifted off the board for `jumpDurationMs`. While airborne, it is completely immune to mid-route collisions.
* **Blocked Landing:** If a non-knight friendly piece occupies the origin square while the jumper is airborne, the landing piece will safely land on an adjacent vacant square (or remain in place if no adjacent square is free).

---

## 4. Collision Mechanics

### A. Mid-Route Collision
Occurs when two pieces move simultaneously and their trajectories intersect at the same square at the same point in time.

* **Immunity:** Knights and airborne jumping pieces (`Airborne`) are entirely immune to route collisions.
* **Between Enemy Pieces (Priority by Initiative):**
  * The engine determines which piece initiated movement earlier (lower `startTime`). That piece is designated as the `winner` (survivor), and the later-moving piece as the `loser` (defeated).
  * The early-moving `winner` **captures** the late-moving `loser`.
  * The losing piece is marked as `Captured` and removed from the board via pointer identity (`PiecePtr`). This identity-based removal eliminates ambiguity and ensures that the engine never accidentally removes the winning piece while both temporarily occupy the same intersection square.
* **Between Friendly Pieces:**
  * The later-arriving piece (`loser`) is blocked, stops at the last vacant square along its path, and enters cooldown.

### B. Destination Arrival Collision
Occurs when a piece completes its trajectory and lands on its target destination square:

* **Friendly Block:** If a friendly piece occupies the target square, the arriving piece stops at the last vacant square along its path and enters cooldown. *(Exception: Knights execute friendly captures/self-kills upon landing rather than being blocked).*
* **Enemy Capture:** If an enemy piece occupies the target square, it is marked as `Captured` and removed from the board.