<div dir="rtl">

# Architecture.md
## Kung-Fu Chess — מסמך ארכיטקטורה כללי

## 1. סקירה כללית ומטרת הפרויקט (High-Level Overview)

Kung-Fu Chess הוא משחק שחמט בזמן אמת (Real-Time, Simultaneous Chess).

בשונה משחמט קלאסי המבוסס על תורות:

- **אין תורות**: שני השחקנים יכולים להזיז כל כלי בכל רגע נתון.
- **זמן צינון (Cooldown)**: לאחר הזזת כלי, הכלי נכנס לתקופת צינון (למשל 2 שניות) שבה אינו יכול לנוע.
- **תנועה ופיזיקה בזמן אמת**: הכלים מחליקים מנקודה לנקודה על פי מהירות מוגדרת.
- **התנגשויות בדרך (Mid-Route Collisions)**: במידה ושני כלים חוצים את אותו תא מסלול בו-זמנית, השרת/מנוע מחשב מי הגיע קודם ומנהל קרב/אכילה באמצע הדרך.
- **מנגנון Premove (קדם-מהלך)**: שחקן יכול לרשום מהלך עתידי לכלי שנמצא כרגע בצינון או בתנועה, והמהלך יבוצע אוטומטית ברגע שהכלי יתפנה.

הפרויקט בנוי ב-C++ מודרני (C++17/20) בארכיטקטורה מבוזרת לקוח-שרת (Client-Server Architecture) התומכת במשחק מקומי (Client-side vs AI / Local), משחק ברשת בזמן אמת, וצפייה בלייב (Spectating).

## 2. ארכיטקטורה ודפוסי תכנון (Architecture & Design Patterns)

המערכת תוכננה לפי עקרונות Clean Architecture ו-SOLID:

### Layered Architecture (ארכיטקטורת שכבות)

- **Core Engine Layer** (`engine/`): מנוע לוגיקה טהור. מנותק לחלוטין מגרפיקה, רשת או קלט. מקבל בקשות (`ActionRequest`), מעבד מצב, ומחזיר תוצאות (`ActionResult`) ואירועים.
- **Server Layer** (`server/`): שרת ייעודי (Dedicated Server) המנהל התחברויות, אימות, matchmaking, ותשדורות רשת ברמה נמוכה.
- **Player Abstraction Layer** (`players/`): הפשטה מלאה של שחקן (`IPlayer`). אין הבדל מנקודת המבט של השרת/מנוע בין שחקן אנושי (`HumanPlayer`), בינה מלאכותית (`GenericAIPlayer`), או שחקן רשת (`NetworkPlayer`).
- **UI & Framework Layer** (`ui/`): מערכת מסכים (Screen Stack) וקומפוננטות UI שאינן תלויות בספרייה גרפית ספציפית.
- **Graphics/Renderer Abstraction Layer** (`graphics/`): תבנית Bridge/Adapter המאפשרת להריץ את הלקוח מעל SFML 3 או מעל OpenCV ללא שינוי בקוד המשחק.

### Design Patterns מרכזיים בפרויקט

- **Strategy Pattern**: מיושם באסטרטגיות ה-AI (`ClassicMinimaxStrategy`, `RealTimeEasy/Medium/HardStrategy`) ובחוקי הכלים (`IPieceRule`).
- **Observer Pattern / Event Bus**: מיושם ב-`EventBus` המרכזי וב-`IGameObserver` לעדכון UI, השמעת צלילים, והקלטת היסטוריה.
- **Factory Pattern**: מיושם ב-`MatchFactory` ליצירת משחקים וב-`PieceRuleFactory` ליצירת חוקי כלים.
- **Command / Request-Response Pattern**: מיושם באמצעות `ActionRequest` ו-`ActionResult` למניעת גישה ישירה ללוח.
- **State Pattern & Screen Stack**: מיושם ב-`ScreenManager` לניהול מעברי מסכים בטוחים (Login → Start → Game).
- **Repository Pattern**: מיושם ב-`IUserRepository`, `SqliteUserRepository`, ו-`InMemoryUserRepository` להפרדת גישה לנתונים.

## 3. הספריות והטכנולוגיות בשימוש (Tech Stack)

| טכנולוגיה / ספרייה | תפקיד במערכת |
|---|---|
| C++17 / C++20 | שפת הפיתוח המרכזית. שימוש נרחב ב-`std::shared_ptr`, `std::optional`, `std::variant`, `std::future`, smart concurrency. |
| Boost.Asio | תשתית התקשורת האסינכרונית בשרת ובלקוח (TCP & UDP), ניהול טיימרים ו-Strands לניהול Concurrency. |
| SFML 3 | הליבה הגרפית הראשית (תקן SFML 3 מודרני): חלונות, קלט, שמע, טקסטורות ורינדור וקטורי/גיאומטרי. |
| OpenCV (`cv::Mat`) | מנוע רינדור וקלט אלטרנטיבי (Fallback/Headless GUI) המאפשר הרצת הלקוח בסביבות ללא SFML. |
| SQLite3 | מסד נתונים קל ומשובץ בשרת לשמירת משתמשים, סיסמאות מוצפנות ודירוגי Elo. |
| Libsodium | ספריית קריפטוגרפיה. משמשת להצפנת סיסמאות בתקן Argon2id (`crypto_pwhash`) העמיד להתקפות Timing & Rainbow tables. |

## 4. תקשורת, רשת וסנכרון (Networking Architecture)

אחד החלקים הקריטיים ביותר בתכנון השרת והלקוח הוא הפרדת ערוצי התקשורת (Dual Transport Split):

### 4.1 ערוץ הבקרה — TCP (Control Channel, Port 8080)

- **מאפיינים**: אמין, מובטח, שומר על סדר (Reliable, Ordered).
- **שימוש**: התחברות (Login), הרשמה (Register), צפייה בחדרים (Lobby/Room List), בקשת הצטרפות למשחק (Matchmaking Request), כניסה כצופה (Spectate), והודעות ניהול סיום משחק (Game Over, Opponent Disconnected).

### 4.2 ערוץ הזמן האמת — UDP (Realtime Channel, Port 8080)

- **מאפיינים**: Low-Latency, Unreliable/Best-effort.
- **שימוש**: תשדורת מהלכים מהירה תוך כדי משחק (`GAME_MOVE`), אישורי מהלך (`MOVE_RESULT`), ושידור דפיקות לב (`HEARTBEAT`).
- **אמינות מעל UDP**: הלקוח מנהל `m_pendingMoves` וטיימר בקרת ניסיונות חוזרים (`checkAndRetryMoves`). אם מהלך שנשלח ב-UDP לא קיבל `MOVE_RESULT` תוך X מילי-שניות, הוא נשלח מחדש עד 5 פעמים.

### 4.3 מנגנון Liveness & Session Binding (`SESSION_BIND`)

כיוון של-UDP אין מושג של "חיבור" (Stateless), תהליך ההתחברות מתבצע כך:

1. הלקוח מתחבר ב-TCP ושולח `LOGIN_REQUEST`.
2. השרת מאמת סיסמה ומחזיר `LOGIN_RESPONSE` עם `sessionToken` ייחודי בן 64-ביט.
3. הלקוח פותח שקע UDP ושולח הודעת `SESSION_BIND` עם ה-`sessionToken`.
4. השרת (`UdpServer` + `SessionManager`) מקשר בין כתובת ה-UDP (`udp::endpoint`) לבין ה-`PlayerSession` הקיים מחוברת ה-TCP, ומחזיר `SESSION_BIND_ACK`.

### 4.4 ניהול Concurrency בשרת (Boost.Asio Strands)

- **ללא Mutexes מרובים למשחק**: כל משחק פעיל (`LiveMatch`) מנהל `boost::asio::strand` משלו.
- **הסבר**: כל אירועי הרשת (מהלך מ-TCP/UDP, טיימר Tick של המשחק, טיימר Disconnect) מועברים לתוך ה-Strand. זה מבטיח שכל לוגיקת ה-Engine של משחק ספציפי רצה סדרתית (Single-threaded execution guarantees per match) ללא סכנת Race Conditions, תוך ניצול מלא של Multi-threading ברמת השרת בין משחקים שונים.

### 4.5 In-Memory State & Matchmaking

- השרת מחזיק בזיכרון ה-RAM את תור ההמתנה (`m_waitingPool` ב-`MatchManager`).
- **מנגנון Matchmaking**: ריצה מחזורית מבוססת טיימר (`kMatchmakingTickInterval = 1s`). המנגנון משווה דירוגי Elo של שחקנים מחכים, ומרחיב את פער ה-Elo המותר ככל שזמן ההמתנה עולה.
- **חדרים פרטיים**: אם נשלח `roomCode != 0`, השחקנים מותאמים אך ורק לפי קוד החדר ולא לפי Elo.

## 5. תהליכים וזרימות נתונים מרכזיות (Data Flows)

### זרימה 1: התחברות (Authentication Flow)

```
Client (LoginScreen) -> AuthService -> [TCP] -> TcpServer -> PlayerSession
  -> SqliteUserRepository (Verify Argon2id via SodiumPasswordHasher)
  -> LOGIN_RESPONSE (Returns token) -> Client
  -> NetworkPlayer (Connects UDP) -> SESSION_BIND -> UdpServer -> SESSION_BIND_ACK
```

### זרימה 2: ביצוע מהלך בזמן אמת (In-Game Move Flow)

```
User Clicks Tile -> ChessGameScreen -> MatchController -> HumanPlayer
  -> NetworkPlayer :: sendMoveToServer()
  -> [UDP NetworkMovePacket] -> UdpServer -> PlayerSession -> LiveMatch (inside Strand)
  -> GameEngine :: processActionRequests()
  -> RealTimeArbiter (Check Cooldown & Motion) -> CollisionResolver / RuleEngine
  -> Return ActionResult (Accepted/Rejected) -> [UDP] -> PlayerSession Sender
  -> Broadcast GAME_MOVE -> [UDP] -> Both Players & Spectators
  -> Client GameEngine :: applyServerMove() -> BoardView Animations & Particle System
```

### זרימה 3: ניתוק והתאוששות (Disconnection & Reconnect Flow)

1. שחקן מנתק את ערוץ ה-TCP/UDP.
2. `LiveMatch` מזהה ניתוק, מסמן `m_isWhiteDisconnected = true`, ומפעיל טיימר ספירה לאחור (למשל 20 שניות).
3. השרת שולח `DISCONNECT_COUNTDOWN` לשחקן השני שמציג Overlay ספירה לאחור.
4. אם השחקן המנותק מתחבר מחדש בזמן (שוב Login עם אותם פרטים), `MatchManager::tryReconnectExistingMatch` מזהה אותו ומחבר אותו חזרה ל-`LiveMatch` הקיים.
5. אם הטיימר אוזל — השרת מפעיל `triggerAutoResign()`, מעניק ניצחון ליריב, ומחשב דירוגי Elo חדשים.

## 6. פירוט קובצי הפרויקט לפי תיקיות (Directory & File Guide)

### `app/` (נקודות כניסה ללקוח)

- `main_sfml.cpp`: נקודת הכניסה הראשית של גרסת SFML 3. מאתחלת חלון, טוענת נכסים (תמונות, גופנים, צלילים), ומריצה את ה-Main Loop.
- `main_opencv.cpp`: נקודת כניסה חלופית המבוססת על OpenCV, משמשת לרינדור ללא SFML.

### `engine/actions/` (בקשות ותוצאות פעולה)

- `PlayerAction.hpp`: מבנה נתונים טהור לייצוג כוונת תנועה (from, to).
- `ActionRequest.hpp`: עוטף `PlayerAction` עם מזהה בקשה (`requestId`) וצבע השחקן.
- `ActionResult.hpp`: תוצאת עיבוד המהלך (Accepted, Rejected, StoredAsPending).

### `engine/analysis/` (אנליזה, AI ו-Elo)

- `EloCalculator.hpp`: חישוב מתמטי של עדכון דירוג Elo לאחר משחק.
- `IMoveGenerator.hpp` / `MoveGenerator.hpp/.cpp`: ייצור כל המהלכים החוקיים התיאורטיים עבור כלי או שחקן מתוך `GameSnapshot`.
- `IPositionEvaluator.hpp` / `PositionEvaluator.hpp/.cpp`: הערכת יתרון/ניקוד של מצב הלוח (ערכי כלים בסנטי-פיונים + איומים).
- `IThreatAnalyzer.hpp` / `ThreatAnalyzer.hpp/.cpp`: ניתוח איומים — בדיקה אם משבצת מסוימת מותקפת על ידי היריב.

### `engine/board/` (ייצוג הלוח והכלים)

- `Piece.hpp/.cpp`: מחלקת הכלי. מחזיקה סוג, צבע, מיקום, מצב (Idle, Moving, Captured, Airborne), ומזהה ייחודי (`id`).
- `IBoard.hpp` / `Board.hpp/.cpp`: מפתח הלוח. מנהל את מטריצת הכלים, הוספה, הסרה והזזה של כלים.

### `engine/common/` (קבועים ומבני נתונים משותפים)

- `Enums.hpp`: הגדרות Enums מרכזיים (`PieceType`, `PlayerColor`, `PieceState`).
- `Position.hpp/.cpp`: מחלקת קואורדינטה לוח (row, col).
- `GameConfig.hpp`: קונפיגורציית פרמטרים של המשחק (זמן צינון, מהירות תנועה במילי-שניות לתא, מאפייני קפיצה).
- `MoveResult.hpp`: מבנה המחזיר האם מהלך התקבל וסיבת דחייה.
- `ArrivalEvent.hpp`: אירוע הגעת כלי ליעד (כולל דגלי אכילה, אכילת מלך, וביטול).
- `BoardPresets.hpp`: מחרוזות מוגדרות מראש של מצב פתיחת הלוח.
- `PieceTokenCodec.hpp`: המרה בינארית/טקסטואלית בין תווים (למשל `'wK'`, `'bP'`) לבין `PieceType` ו-`PlayerColor`.
- `PieceValues.hpp`: ערכי כלים סטנדרטיים (למשל Pawn=1, Queen=9) וערכים בסנטי-פיונים עבור AI.

### `engine/core/` (ליבת מנוע המשחק)

- `IGameEngine.hpp`: ממשק מנוע המשחק.
- `GameEngine.hpp/.cpp`: מנוע המשחק המרכזי. מקשר בין הלוח, חוקי המשחק, ה-Arbiter וה-PremoveQueue. מריץ את לולאת הזמן (`wait(ms)`).
- `MatchController.hpp/.cpp`: Controller מרכזי ברמת ה-Application. מאחד את מנוע המשחק, השחקן האנושי, ה-AI/Network, ניהול הפאוס, והיסטוריית מהלכים.
- `PremoveQueue.hpp/.cpp`: תור קדם-מהלכים. מאפשר לשחקן לרשום מהלך עתידי לכלי שנמצא בצינון, ומבצע אותו ברגע שהכלי מתפנה.

### `engine/events/` (אירועים והקלטה)

- `GameEvents.hpp`: הגדרת אירועי EventBus (`MoveCompletedEvent`, `ScoreChangedEvent`, `PlaySoundEvent`, `GameTransitionEvent`).
- `IGameObserver.hpp`: ממשק Observer להאזנה לסיום תנועת כלים.
- `MoveHistoryTracker.hpp`: מקליט ומפרסם את היסטוריית המהלכים בציון זמן מדויק בפורמט שחמט.

### `engine/io/` (פארסינג והדפסה)

- `BoardParser.hpp/.cpp`: המרת מחרוזת טקסטואלית ל-`Board` חי.
- `BoardPrinter.hpp/.cpp`: המרת `Board` חי למחרוזת טקסטואלית (לצורך דיבוג וסנכרון צופים).

### `engine/realtime/` (פיזיקה, צינון ותנועה בזמן אמת)

- `Motion.hpp/.cpp`: מייצג תנועה אקטיבית של כלי באוויר/על הלוח (`from`, `to`, `startTime`, `arrivalTime`).
- `CooldownTracker.hpp`: מעקב אחר זמני תפוגת הצינון של כל כלי לפי ה-`id` שלו.
- `CollisionDetector.hpp/.cpp`: גילוי התנגשויות באמצע הדרך (Broad-phase bounding box & Narrow-phase path steps).
- `RealTimeArbiter.hpp/.cpp`: "הבורר" בזמן אמת. מנהל את קדימות התנועות, מתקדם בזמן, ומפעיל את ה-CollisionResolver.

### `engine/rules/` (חוקי המשחק)

- `IPieceRule.hpp` / `PieceRules.hpp/.cpp`: החוקים הגיאומטריים של כל כלי (צריח, רץ, פרש, מלכה, מלך, רגלי).
- `IPromotionRule.hpp` / `PromotionRules.hpp/.cpp`: חוקי הכתרת רגלי (Promotion).
- `CollisionResolver.hpp/.cpp`: פתרון פגיעות והתנגשויות (מנצח/מפסיד בהתנגשות באמצע המסלול, או עצירה במשבצת הפנויה האחרונה).
- `RuleEngine.hpp/.cpp`: מנוע אימות חוקיות מהלכים (בדיקת גבולות, חסימות, וחוקי הכלים).

### `engine/snapshot/` (Snapshots לרינדור ו-AI)

- `GameSnapshot.hpp`: תמונת מצב קפואה, מנותקת מזיכרון הלוח, המכילה את כל הכלים והסטטוסים.
- `SnapshotBuilder.hpp/.cpp`: בונה `GameSnapshot` מתוך ה-`Board` וה-`RealTimeArbiter`.

### `graphics/opencv/` (מימוש רינדור ב-OpenCV)

- `img.hpp/.cpp`: מעטפת נוחה מעל `cv::Mat` לטעינה, ציור, Blend אלפא, וטקסט.
- `ImgRenderer.hpp`: מימוש `IRenderer` עבור OpenCV (כולל Rounded Rectangles, Shadow, Glow, Glass Panel fallbacks).
- `ImgInputTranslator.hpp`: תרגום אירועי עכבר ומקלדת של OpenCV ל-`InputEvent`.

### `graphics/sfml/` (מימוש רינדור ב-SFML 3)

- `SfmlAssets.hpp`: טעינת טקסטורות וגופנים של SFML.
- `SfmlRenderer.hpp`: מימוש `IRenderer` עשיר מעל SFML 3 (תמיכה בגרדיאנטים, צלליות, הילה, ושלטי זכוכית).
- `SfmlInputTranslator.hpp`: תרגום אירועי SFML 3 ל-`InputEvent`.
- `SfmlSoundPlayer.hpp`: מנוע השמעת צלילים ואפקטים בלולאה מעל SFML Audio.

### `players/` (שחקנים ושירותים)

- `IPlayer.hpp`: ממשק השחקן הכללי.

#### `players/ai/`

- `IAIDecisionStrategy.hpp`: ממשק אסטרטגיית ה-AI.
- `ClassicMinimaxStrategy.hpp/.cpp`: אלגוריתם Minimax עם גיזום Alpha-Beta למצב קלאסי.
- `RealTimeBaseStrategy.hpp` / `RealTimeStrategies.hpp`: אסטרטגיות בזמן אמת לרמות Easy, Medium, Hard (הערכת איומים ותקיפה מקבילית).
- `GenericAIPlayer.hpp`: מימוש `IPlayer` המריץ אסטרטגיה ומחזיר `ActionRequest`.

#### `players/human/`

- `InputConfig.hpp`: קבועי קלט (גודל תא דיפולטיבי).
- `BoardMapper.hpp/.cpp`: המרת פיקסלים מאירוע עכבר לקואורדינטות הלוח.
- `Controller.hpp/.cpp`: ניהול בחירת כלים וקליקים של שחקן אנושי.
- `HumanPlayer.hpp/.cpp`: עוטף את ה-`Controller` כ-`IPlayer`.

#### `players/network/`

- `ClientAuth.hpp`: שמירת נתוני ה-Session של השחקן המחובר בלקוח.
- `ClientConfig.hpp`: קונפיגורציית הלקוח (כתובות שרת, טיימרים, Retries).
- `AuthService.hpp/.cpp`: שירות אסינכרוני המבצע התחברות/הרשמה מול השרת.
- `LobbyService.hpp/.cpp`: שירות ברקע המביא את רשימת החדרים הפעילים בלייב.
- `NetworkPlayer.hpp/.cpp`: המנהל המרכזי של תקשורת הלקוח. מנהל שקע TCP ושקע UDP, תור מהלכים ממתינים, ושידור מחדש.
- `NetworkSession.hpp`: מחזיק חיבור רשת פעיל ועובד לאורך מעברי מסכים.

### `server/` (צד השרת)

- `ServerConfig.hpp`: הגדרות שרת (פורטים 8080 TCP/UDP, טיימרים, פער Elo התחלתי).
- `main.cpp`: נקודת הכניסה של השרת. מאתחל DB, `MatchManager`, `SessionManager`, ומפעיל את ה-Event Loop של Asio.

#### `server/match/`

- `LiveMatch.hpp/.cpp`: מנהל משחק יחיד רץ. מריץ Tick Loop בתוך Strand, מטפל בהתנגשויות, ומנהל ניתוק/חיבור מחדש.
- `MatchFactory.hpp/.cpp`: Factory ליצירת אובייקט `LiveMatch` מוגדר כהלכה.
- `MatchManager.hpp/.cpp`: מנהל ה-Matchmaking המרכזי. מחזיק את `m_waitingPool`, מריץ מחזור שידוך, ומחשב ניקוד Elo בסיום.

#### `server/network/`

- `NetworkMessages.hpp`: הפרוטוקול הבינארי — סוגי ההודעות, שיוך ל-Transport (TCP vs UDP), ומבני הנתונים של ה-Wire.
- `IConnection.hpp`: ממשק אבסטרקטי לשליחת הודעות רשת.
- `TcpConnection.hpp/.cpp`: ניהול חיבור TCP יחיד (Read header -> Read payload -> Write queue).
- `TcpServer.hpp/.cpp`: שרת ה-TCP המקבל חיבורים חדשים ויוצר `PlayerSession`.
- `UdpConnection.hpp/.cpp`: ניהול נקודת קצה UDP מול שחקן יחיד.
- `UdpServer.hpp/.cpp`: שרת ה-UDP המקבל את כל הדיאגרמות, מפעיל `SESSION_BIND`, ומנתב הודעות ל-Sessions.
- `Serializer.hpp`: סריאליזציה/דה-סריאליזציה בינארית בתקן Big-Endian לכל הודעות הפרוטוקול.
- `PlayerSession.hpp/.cpp`: ייצוג זהות שחקן מחובר בשרת. מחזיק ערוץ TCP וערוץ UDP bound.
- `SessionManager.hpp/.cpp`: אוגר מרכזי שמקשר בין tokens, ערוצי UDP ו-Sessions.

#### `server/persistence/`

- `UserRecord.hpp`: DTO המייצג משתמש (שם, האש סיסמה, דירוג Elo).
- `IUserRepository.hpp`: ממשק גישה לנתוני משתמשים.
- `InMemoryUserRepository.hpp`: מימוש בזיכרון (למוק/טסטים).
- `SqliteUserRepository.hpp/.cpp`: מימוש ה-DB האמיתי מבוסס SQLite3.
- `PasswordHasher.hpp/.cpp`: הצפנת סיסמאות בעזרת Libsodium (Argon2id).

### `ui/` (ממשק משתמש וקומפוננטות)

- `ui/theme/Theme.hpp`: פלטת צבעים, גדלים, רדיוסים וגרדיאנטים מרכזיים.

#### `ui/animations/`

- `Animation.hpp`: פונקציות Easing (Linear, Smoothstep, Parabola), אינטרפולציית מיקום/קפיצה, ובלנד צבעים.
- `HoverGroup.hpp`: מנהל מעברי Hover חלקים לקומפוננטות UI.

#### `ui/components/`

- `Button.hpp`: כפתור מודרני כולל צלליות, אפקט Glow, גרדיאנטים ואנימציית Hover.
- `BoardView.hpp`: הקומפוננטה המרכזית המרנדרת את הלוח, המשבצות, הכלים הנעים, גזרות Cooldown, וסמני Premove.
- `CooldownBar.hpp`: פס צינון חזותי.
- `HeaderView.hpp` / `FooterView.hpp` / `SidebarView.hpp`: פאנלים היקפיים לטייטל, ניקוד, וכתיבת היסטוריית המהלכים.
- `GameOverlaysView.hpp`: שכבות תצוגה מעל הלוח (Matchmaking, Pause, Disconnect Countdown, Game Over).
- `ParticleSystem.hpp`: מערכת חלקיקים לאפקטי פיצוץ ואבק כוכבים בעת אכילת כלי או ניצחון.

#### `ui/framework/`

- `IRenderer.hpp`: ממשק הציור המופשט.
- `IInputTranslator.hpp` / `InputEvents.hpp`: תרגום אירועי קלט לתקן אחיד.
- `ISoundPlayer.hpp`: ממשק מנגן צלילים (כולל `NullSoundPlayer`).
- `AssetManager.hpp`: טעינת משאבים מופשטת בלייזי לפי Type Erasure (`IAsset`).
- `EventBus.hpp`: ניהול אירועים פנימי, Thread-safe.
- `IScreen.hpp` / `ScreenManager.hpp`: מנהל המסכים במבנה תור/מחסנית (Push/Pop/Change).

#### `ui/screens/`

- `BaseScreen.hpp/.cpp`: מסך בסיס הכולל רקע דינמי משתנה, פאנלים של זכוכית (Glassmorphism), ועיטורים.
- `LoginScreen.hpp/.cpp`: מסך התחברות/הרשמה/משחק אופליין.
- `StartScreen.hpp/.cpp`: מסך תפריט ראשי, בחירת מצב משחק (Real-time / Classic), יריב (Local / AI / Online), רמה וחדרים.
- `ChessGameScreen.hpp/.cpp`: מסך המשחק הראשי המאחד את ה-`MatchController` והקומפוננטות הגרפיות.

## 7. דגשים קריטיים לתכנון העתידי: דוקר (Docker), ענן והרחבה

> להרחבה ופירוט מלא של הנקודות הבאות עבור סקאלה גלובלית (100M משתמשים, 10M concurrent), ראו את מסמך [`Server_Design.md`](./Server_Design.md).

אם ברצונך להעלות את השרת לדוקר/ענן (AWS/GCP/Azure) או להרחיב אותו, להלן הנקודות המרכזיות שישפיעו על התכנון:

### 7.1 ניהול הפורטים בדוקר (Docker Port Mapping)

השרת משתמש בפורטים מקבילים: 8080 TCP (בקרה) ו-8080 UDP (זמן אמת).

בקובץ `docker-compose.yml` או ב-`Dockerfile` חובה לחשוף את שניהם מפורשות:

```yaml
ports:
  - "8080:8080/tcp"
  - "8080:8080/udp"
```

### 7.2 אופי השרת — Stateful Server

השרת שומר מצב בזיכרון: ה-`MatchManager` מחזיק את המשחקים הפעילים (`LiveMatch`) ואת תור הממתינים (`m_waitingPool`) בזיכרון ה-RAM.

**משמעות לענן**:

- לא ניתן פשוט לשים Load Balancer רגיל ולהרים 5 instances של השרת ללא מנגנון תיאום — כי שחקן א' יהיה בשרת 1 ושחקן ב' בשרת 2 והם לא יראו זה את זה ב-waitingPool.
- **פתרונות לעתיד**:
  - הרצת Instance יחיד חזק (Single Dedicated Server Instance).
  - במידה ורוצים Scale-Out: הפרדת רכיב ה-Matchmaking וה-Lobby לשירות נפרד (למשל בעזרת Redis Pub/Sub), ואז הפניית שני השחקנים ל-Game Server יחיד שיארח את ה-`LiveMatch` שלהם.

### 7.3 מסד הנתונים (SQLite3 vs External DB)

כרגע השרת משתמש ב-`SqliteUserRepository` השומר קובץ מקומי בשם `kungfu_chess.db`.

בדוקר, חובה להגדיר Docker Volume לקובץ ה-DB כדי שהנתונים והמשתמשים לא יימחקו בעת הפעלה מחדש של ה-Container:

```yaml
volumes:
  - sqlite_data:/app/data
```

בסקייל רחב בענן, ניתן להחליף את מימוש ה-`IUserRepository` ל-PostgreSQL/MySQL (או NoSQL, ראו `Server_Design.md`) על ידי יצירת קובץ חדש (למשל `PostgresUserRepository.cpp`) מבלי לגעת באף שורה ב-`MatchManager` או ב-`PlayerSession` (בזכות ה-Dependency Inversion Principle).

### 7.4 תאימות סביבות (Libsodium & Boost Dependencies)

- השרת תלוי ב-`boost_system`, `sqlite3`, ו-`libsodium`.
- בבניית קובץ ה-Dockerfile יש להשתמש ב-Multi-stage build (למשל תמונה עם `g++`, `cmake`, `libboost-dev`, `libsodium-dev`, `libsqlite3-dev` לבנייה, ותמונה רזה להרצה).

## סיכום

פרויקט Kung-Fu Chess מתוכנן ברמה הנדסית גבוהה מאוד:

- מנוע ה-Engine טהור ומנותק מתקשורת/תצוגה.
- תקשורת היברידית קלאסית של משחקי זמן אמת (TCP לבקרה + UDP למשחקיות).
- Concurrency בטוח ומהיר באמצעות Boost.Asio Strands מונע נעילות Mutexes מורכבות.
- המבנה המודולרי מאפשר הרחבה קלה של אלגוריתמי AI, מסדי נתונים, או מנועי רינדור גרפיים בעתיד.

</div>
