# Real-time mechanics, cooldowns, and collisions

קובץ זה מתעד את חוקי הפיזיקה וזמן האמת הייחודיים המיושמים בפרויקט. חוקים אלו נאכפים על ידי ה-`RealTimeArbiter` בשיתוף עם ה-`CollisionDetector` וה-`CollisionResolver`.

## 1. Continuous motion
* **Movement speed:** pieces move over time rather than jumping immediately to their destination. The movement duration is calculated from the path distance multiplied by the speed (`msPerCellSpeed`, default: 1000ms per square).
* **Interpolation:** the piece position is updated continuously by a time-based calculation (`interpolatePosition`), which allows the UI in the future to display smooth animation.

## 2. Cooldown timing
* **Cooldown Tracker:** after a move is made, the piece enters a cooldown period (`cooldownDurationMs`, default: 2000ms).
* **Busy piece:** a piece that is moving or in cooldown is considered busy and cannot receive a new movement request directly (unless it is a premove).

## 3. Jump in place (airborne state)
* **Activation:** sending a movement request for a piece to the square it is currently on (`from == to`) triggers a jump in place.
* **Temporary immunity:** the piece transitions to the `PieceState::Airborne` state and is removed logically from the board for `jumpDurationMs` (default: 1000ms). During this time it cannot be affected by route collisions.
* **Blocked landing:** if a friendly piece (not a knight) enters its square while it is in the air, the piece will land on a nearby empty square (or remain in its original place if no empty square is available).

## 4. Collision mechanics

### A. Mid-route collision
This occurs when two pieces move simultaneously and their paths intersect at the same square and the same time.
* **Immunity:** knights and pieces in the airborne jump state (`Airborne`) are completely immune to route collisions.
* **Between enemies (early initiative):**
  * The engine identifies which piece started earlier (`startTime` lower) [1]. That piece is classified as the `winner` (survivor), and the piece that started later is classified as the `loser` (defeated) [1].
  * The piece that started first (`winner`) **captures** the piece that started second (`loser`) [1]. 
  * הכלי המפסיד מסומן כנלכד (`Captured`) ומוסר מהלוח לפי הזהות הפיזית שלו (`PiecePtr`) [1]. מחיקה מבוססת זהות זו מונעת עמימות ומבטיחה שהלוח לא ימחק בטעות את הכלי המנצח כאשר שניהם נמצאים זמנית באותה משבצת מפגש [1].
* **בין ידידים:**
  * הכלי שהגיע מאוחר יותר (`loser`) נחסם, נעצר במשבצת הפנויה האחרונה במסלולו, וזמן צינון מוחל עליו.

### ב. התנגשות בהגעה ליעד (Arrival Collision)
מתרחשת כאשר כלי מסיים את תנועתו ומגיע ליעד:
* **חסימת ידיד (Friendly Block):** אם ביעד עומד כלי ידידותי, הכלי המגיע נעצר במשבצת הפנויה האחרונה שלפניו ונכנס לצינון.(למעט פרש, שלו יש אופצית השמדת כלי ידידותי)
* **הכאת אויב:** אם ביעד עומד כלי אויב, הוא מסומן כנלכד (`Captured`) ומוסר מהלוח.