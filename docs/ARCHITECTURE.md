Kung Fu Chess
מסמך ארכיטקטורה — מבנה תיקיות סופי ושכבת Player/AI
1. מטרת המסמך
מסמך זה מגדיר את מבנה התיקיות הסופי של הפרויקט, לאחר שילוב עקרונות הארכיטקטורה הקיימים (Architecture Principles, Command System, Player API Design) עם שכבת שחקן (Player) הכוללת תמיכה מובנית ב-AI. לכל תיקייה ולכל קובץ חדש מוגדרת אחריות ברורה ויחידה, בהתאם לעקרון "Single Source of Truth" ולעקרון "Components Own One Responsibility".
המסמך מהווה המשך ישיר למסמכי הארכיטקטורה הקיימים, ואינו סותר אותם — הוא ממקד את יישומם במבנה תיקיות קונקרטי.
2. עקרון־העל: שכבות ותלויות
המערכת בנויה משלוש שכבות עיקריות, כאשר כיוון התלות הוא חד־כיווני בלבד — משכבה חיצונית פנימה, ולעולם לא ההפך:
•	engine — שכבת הסימולציה הסמכותית. אינה מכירה את קיומם של players, ui או graphics.
•	players — מימושי IPlayer (Human, AI, Network, Replay). תלויים אך ורק ב-API הציבורי של engine: snapshot ו-actions.
•	ui / graphics — תצוגה בלבד. תלויים גם הם רק ב-snapshot של engine, ולעולם לא בלוגיקת המשחק.
כלל הברזל: כל קובץ בתוך engine/ מכיר רק קבצים אחרים בתוך engine/. שום קובץ ב-engine/ אינו כולל (include) קובץ מ-players/, ui/ או graphics/. הכיוון ההפוך — players/ ו-ui/ הכוללים קבצים מ-engine/ — הוא הכיוון החוקי היחיד.
3. עץ התיקיות הסופי
src/
├── app/                     ← composition root
│   ├── main_opencv.cpp
│   └── main_sfml.cpp
│
├── engine/                  ← שכבת הסימולציה הסמכותית
│   ├── core/                GameEngine, IGameEngine, PremoveQueue
│   ├── board/               Board, IBoard, Piece
│   ├── common/              Position, Enums, GameConfig, MoveResult, PieceTokenCodec, ArrivalEvent
│   ├── rules/               RuleEngine, PieceRules, PromotionRules, CollisionResolver
│   ├── analysis/     [חדש]  MoveGenerator, ThreatAnalyzer, PositionEvaluator
│   ├── realtime/            Motion, CooldownTracker, CollisionDetector, RealTimeArbiter
│   ├── io/                  BoardParser, BoardPrinter
│   ├── events/              IGameObserver, MoveHistoryTracker
│   ├── actions/      [חדש]  PlayerAction, ActionRequest, ActionResult
│   └── snapshot/            GameSnapshot, SnapshotBuilder
│
├── players/         [חדש]  ← מימושי IPlayer
│   ├── IPlayer.hpp
│   ├── human/               HumanPlayer, Controller, BoardMapper, InputConfig
│   ├── ai/                  AIPlayer, EasyAI, MediumAI, HardAI
│   ├── network/             (שריון לעתיד)
│   └── replay/              (שריון לעתיד)
│
├── graphics/                ← backends גרפיים
│   ├── opencv/
│   └── sfml/
│
├── ui/
│   ├── components/
│   ├── framework/
│   └── screens/             ChessGameScreen, StartScreen
│
└── tests/
 4. טבלת אחריויות — תיקיות חדשות ומשתנות
תיקייה	אחריות	תלוי ב-
engine/actions	הגדרת חוזה הבקשה: PlayerAction, ActionRequest, ActionResult (Accepted / Rejected / StoredAsPending). לא מכיל לוגיקה, רק טיפוסי נתונים.	engine/common
engine/analysis	פונקציות טהורות (Pure Functions) הפועלות על GameSnapshot: ייצור מהלכים חוקיים, ניתוח איומים, הערכת מיקום. אינן משנות מצב.	engine/rules (דרך ממשק קריאה בלבד), engine/snapshot
engine/snapshot	בניית תמונת מצב לקריאה בלבד (GameSnapshot) הכוללת את כל הנתונים הדרושים ל-analysis וללא צורך בגישה ל-Board החי.	engine/board, engine/common
engine/events	מנגנון התראות Push (IGameObserver) למעקב אחר היסטוריית מהלכים ואירועים, בנפרד מה-Snapshot שהוא Pull.	engine/core
players/IPlayer.hpp	ממשק אחיד לכל סוגי השחקנים: מקבל GameSnapshot, מחזיר אוסף ActionRequest.	engine/snapshot, engine/actions
players/human	תרגום קלט גולמי (עכבר / מקלדת) ל-ActionRequest. מכיל את מה שהיה core/input.	players/IPlayer.hpp, engine/actions
players/ai	מימושי AI המשתמשים ב-engine/analysis לבניית החלטה, ולעולם לא ב-engine/board או ב-engine/core ישירות.	players/IPlayer.hpp, engine/analysis, engine/snapshot, engine/actions
app	נקודת הכניסה היחידה שמכירה את כל השכבות ומרכיבה אותן יחד (Dependency Injection).	engine, players, ui, graphics
5. מה מותר ומה אסור לשחקן (כולל AI)
רכיב	Player יכול לגשת?	הסיבה
GameSnapshot	כן	תמונת מצב לקריאה בלבד
engine/analysis (MoveGenerator וכו')	כן	פונקציות טהורות, לא משנות מצב
engine/actions (ActionRequest)	כן — יוצר ושולח	זהו ערוץ הכתיבה החוקי היחיד
engine/board (Board)	לא	רק GameEngine רשאי לשנות את הלוח
engine/core (GameEngine::executeMove)	לא	GameEngine הוא הסמכות היחידה לשינוי מצב
engine/rules (RuleEngine ישירות)	לא	הגישה לחוקיות מתבצעת דרך analysis בלבד, כדי לשמור מקור אמת יחיד
6. תזרים קבלת החלטה של AI
כל תור חישוב של AIPlayer עובר את השרשרת הבאה:
•	GameEngine בונה GameSnapshot עדכני (Read-Only).
•	AIPlayer מעביר את ה-Snapshot ל-MoveGenerator, שמייצר את כל המהלכים החוקיים תוך שימוש חוזר בלוגיקת RuleEngine הקיימת — לא לוגיקה כפולה.
•	ThreatAnalyzer ו-PositionEvaluator מדרגים את המהלכים החוקיים.
•	AIPlayer בוחר פעולה ומחזיר ActionRequest אחד או יותר.
•	GameEngine מקבל את הבקשה ומאמת אותה מחדש (חוקיות, מצב הכלי, Cooldown) — גם אם כבר נבדקה על ידי ה-AI, כי מצב הלוח יכול להשתנות עד לרגע הביצוע בפועל (משחק בזמן אמת).
•	רק לאחר אימות שני מתבצע השינוי בפועל בלוח.
האימות הכפול אינו כפילות מיותרת: הוא המנגנון ששומר על Principle 12 (Deterministic Simulation) ועל Principle 2 (GameEngine הוא הסמכות היחידה) גם כאשר קיימים מספר שחקנים החושבים ופועלים במקביל.
7. מיפוי הגירה מהמבנה הקיים
מיקום נוכחי	מיקום חדש	הערה
core/engine (GameEngine, IGameEngine, PremoveQueue)	engine/core	ללא שינוי בתוכן
core/engine (IGameObserver, MoveHistoryTracker)	engine/events	הופרד מ-core כדי להבחין בין ליבת המנוע לבין מנגנון ההתראות
core/board	engine/board	ללא שינוי
core/common	engine/common	ללא שינוי
core/input (Controller, BoardMapper, InputConfig)	players/human	מעבר לוגי: זהו בדיוק תרגום קלט ל-ActionRequest, אחריות של HumanPlayer
core/io	engine/io	ללא שינוי
core/realtime	engine/realtime	ללא שינוי
core/rules (RuleEngine, PieceRules, PromotionRules, CollisionResolver)	engine/rules	ללא שינוי
core/rules/MaterialEvaluator	engine/analysis/PositionEvaluator	שינוי שם ומיקום: הערכת מיקום אינה חוק משחק
core/view (GameSnapshot, SnapshotBuilder)	engine/snapshot	ללא שינוי בתוכן
core/view/screens (ChessGameScreen, StartScreen)	ui/screens	הוצא מ-engine לגמרי — אלו קבצי תצוגה
graphics_impl	graphics	שינוי שם בלבד
main_opencv.cpp, main_sfml.cpp	app/	רוכזו בשכבת composition root ייעודית
— (לא קיים)	engine/actions	תיקייה חדשה: PlayerAction, ActionRequest, ActionResult
— (לא קיים)	engine/analysis/MoveGenerator, ThreatAnalyzer	תיקייה חדשה
— (לא קיים)	players/ (כולל IPlayer, human, ai, network, replay)	שכבה חדשה במלואה
8. הרחבות עתידיות אפשריות ללא שינוי בליבה
•	players/network — NetworkPlayer שמקבל ActionRequest מרחוק ומזרים אותו לאותו IPlayer API.
•	players/replay — ReplayPlayer שמזין מחדש רצף ActionRequest שנשמר מראש (Deterministic Simulation מאפשר זאת).
•	players/ai/HardAI, MediumAI, EasyAI — רמות קושי שונות, כל אחת מימוש נפרד של IPlayer, ללא נגיעה ב-GameEngine.
•	engine/analysis — ניתן להחליף מימוש MoveGenerator (למשל למימוש מבוסס bitboards למהירות) ללא שינוי ב-players, כל עוד הממשק IMoveGenerator נשמר.
•	graphics — הוספת backend גרפי נוסף כמימוש חדש של IRenderer, ללא שינוי במנוע או בשחקנים.
