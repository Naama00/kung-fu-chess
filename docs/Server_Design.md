<div dir="rtl">

# Server_Design.md
## Kung-Fu Chess — תכנון שרת לסקאלה (Cloud & Kubernetes Design)

> מסמך זה הוא המשך למסמך הארכיטקטורה הכללי [`Architecture.md`](./Architecture.md), ומתמקד אך ורק בהרחבת השרת לתמיכה בעומס גלובלי (Scale). לפרטי הארכיטקטורה הבסיסית (Engine, Networking, Design Patterns) יש לעיין שם.

מסמך זה עוסק בהרחבת ארכיטקטורת השרת הקיימת (Boost.Asio, Strand-per-match, SQLite, TCP+UDP Dual Transport) לתמיכה בעומס בקנה מידה גלובלי: **100,000,000 משתמשים רשומים**, **10,000,000 משתמשים פעילים בו-זמנית**. הדגש הוא על **Design של מערכת ענן** — לא שינוי בלוגיקת המשחק/ה-Engine עצמם, אלא השכבות שמסביבם.

---

## 1. שכבת הנתונים (Data Layer) — Users DB

### הבעיה עם SQLite בסקאלה
`SqliteUserRepository` הקיים מתאים לפריסה מקומית/יחידה, אך **אינו מתאים** לארכיטקטורה מבוזרת:
- SQLite הוא embedded, single-file, עם file-level locking — לא בנוי לכתיבה בו-זמנית ממספר תהליכים/מכונות שונות.
- כאשר יש מספר instances של game-server הרצים על מכונות נפרדות, אין אפשרות שכולם יגשו לאותו קובץ SQLite בו-זמנית בצורה בטוחה ומהירה.

### דרישות בפועל מה-DB
- גישה מסוג **key-value** בעיקרה: `user_id → {password_hash, username, elo}`.
- אין קשרים (Relations/JOINs) מורכבים — טבלה אחת עצמאית.
- **אין צורך ב-Consistency קשיח (ACID)** — עדכון Elo בסיום משחק לא חייב להיות אטומי-מיידי-גלובלי; Eventual Consistency מספיקה.
- נדרשת מהירות read/write ו**Scale אופקי אמיתי על פני datacenters** (100M משתמשים, 10M concurrent מכל העולם).

### החלטה
מעבר מ-Relational DB (SQLite) ל-**NoSQL Key-Value / Wide-Column DB** (למשל **DynamoDB** מנוהל, או **Cassandra/ScyllaDB** open-source):
- Scale אופקי ליניארי — הוספת nodes לפי צורך.
- Eventual consistency כברירת מחדל, עם אופציה ל-strong consistency בשאילתה בודדת היכן שנדרש (כמו עדכון Elo).
- פריסה טבעית על פני מספר datacenters גלובליים.

### השפעה על הקוד הקיים
בזכות ה-**Repository Pattern** שכבר קיים (`IUserRepository`), המעבר הוא **תוסף חדש בלבד**:
- יצירת `DynamoUserRepository` / `CassandraUserRepository` חדש, מימוש הממשק הקיים.
- **אין צורך לגעת** ב-`MatchManager`, `PlayerSession`, או כל קוד אחר שצורך את ה-Repository — תודות ל-Dependency Inversion Principle שכבר תוכנן מראש.

---

## 2. שכבת ה-Matchmaking וה-Routing — "מי משחק עם מי, ואיפה"

### הבעיה
כאשר יש מאות/אלפי instances של game-server (Docker containers), כל אחד מהם מחזיק היום `MatchManager` עצמאי עם `m_waitingPool` מקומי בזיכרון. שני שחקנים שמתחברים ל-containers שונים **לא רואים זה את זה**, ואין דרך "למצוא" משחק שכבר רץ על container אחר.

### פתרון: הפרדת אחריות לשלוש שכבות

```
Client → Gateway / Load Balancer (TCP entrypoint)
              ↓
      Matchmaking Service (stateless containers, waitingPool משותף ב-Redis)
              ↓ (מוצא זיווג לפי Elo, מקצה Game-Server instance פנוי)
      Game-Server Instance (מריץ בתוכו הרבה LiveMatch objects, כל אחד ב-Strand משלו)
              ↑
      Session Registry (Redis): match_id → server_ip:port , player_id → match_id
```

- **Matchmaking Service**: שכבה נפרדת מ-game-server עצמו. ה-`waitingPool` (היום in-memory ב-`MatchManager`) עובר ל-**Redis משותף**, כך שכל instance של שירות ה-Matchmaking רואה את אותו מאגר ממתינים, ללא תלות במספר ה-instances. חדרים פרטיים (`roomCode != 0`) ממשיכים להתאים לפי קוד בלבד, בדיוק כמו היום.
- **Session Registry** (Redis): טבלת מיפוי מהירה בזיכרון בין `match_id` לכתובת ה-container המריץ אותו, ובין `player_id` ל-`match_id` הפעיל שלו. זהו הבסיס לכל בקשת Spectate או Reconnect — הלקוח פונה קודם ל-Gateway, שבודק ב-Registry "איפה המשחק רץ", ומפנה/עושה proxy לשרת הנכון.
- **Game-Server Instance**: כאן ה-`LiveMatch` וה-Strand-per-match הקיימים **נשארים בדיוק כפי שהם** — האחריות שלהם היא רק ניהול משחק בודד, ללא שינוי.

### התאוששות מקריסת Container (Fault Tolerance)
כאשר container שמריץ משחקים קורס:
1. Kubernetes (health checks) מזהה את הנפילה, ומרים instance חדש (Self-healing).
2. מנגנון ה-**Reconnect** הקיים (`tryReconnectExistingMatch`) הוא הצינור הנכון בצד הלקוח — אך יש לעדכן את כתובת היעד: הלקוח לא מנסה להתחבר מחדש לכתובת הישנה, אלא פונה ל-Session Registry שמפנה לכתובת (container) חדשה.
3. **קריטי**: state של משחק חי (מיקומי כלים, `Motion` באוויר, `CooldownTracker`, `PremoveQueue`) קיים כרגע **רק ב-RAM** של container בודד — ולכן שביר לחלוטין בקריסה.

### מנגנון Snapshot תקופתי (Persistence for Live State)
כדי שקריסת container לא תגרום לאובדן משחקים פעילים (במיוחד לאור העובדה שאין שום צידוק לחוויית "המשחק נגמר כי השרת קרס"):
- כל **5 שניות** (frequency ניתנת לכיוונון לפי עומס/עלות), נשמר **`GameSnapshot`** מלא ל-Redis/מאגר חיצוני מהיר.
- **חשוב**: ה-Snapshot חייב לכלול לא רק מיקומי כלים סטטיים, אלא גם את **ה-timestamps היחסיים** — `startTime`/`arrivalTime` של `Motion` פעיל, זמני תפוגת `CooldownTracker`, ותור `PremoveQueue` ממתין. אחרת, כלי "באוויר" ברגע ה-crash "יקפוץ" למקום מוזר כאשר משחק "מורם" מחדש על container אחר, והתזמון העדין שהוא ה-USP של המשחק (Cooldown, Motion חלק) יתקלקל.
- ברגע שמזוהה שמשחק "אבד" (container קרס), instance אחר טוען את ה-Snapshot האחרון וממשיך את ה-`LiveMatch` מהנקודה ההיא.

---

## 3. עומס תעבורת רשת (Network Traffic Load)

### נתוני יסוד
- שחקן פעיל מבצע מהלך בממוצע כל **2 שניות**.
- 10,000,000 משתמשים פעילים בו-זמנית.
- הפרויקט כבר משתמש בשידור **Delta** (רק השינוי שבוצע — piece id, from, to, timestamp) ולא ב-Snapshot מלא של הלוח בכל מהלך — אופטימיזציה קריטית שכבר מיושמת ב-`Serializer`/`NetworkMessages`.

### חישוב
- גודל הודעת `GAME_MOVE` בודדת (Delta, Big-Endian): **~20–30 בתים**.
- הודעות נכנסות לשנייה: 10,000,000 × (1/2) = **5,000,000 הודעות/שנייה**.
- עם broadcast ליריב (ובצופים, כשיש): בקירוב **פי 2** → **~10,000,000 הודעות/שנייה**.
- סה"כ תעבורה: 10,000,000 × ~25 בתים = **250 MB/s ≈ 2 Gbps** על פני **כל המערכת**.

### מסקנה
2 Gbps גלובלי הוא **נמוך ביחס לתשתיות backbone** של ספקי ענן (שמדברות במונחי 10–100 Gbps לשרת בודד, ו-Terabits לכל datacenter). כאשר מפזרים את זה על פני מאות containers, ה-bandwidth הנדרש **לכל container בודד** הוא זניח (בסדר גודל של יחידות Mbps). המסקנה המעשית: **Bandwidth אינו צוואר הבקבוק** של המערכת — הוא תוצאה ישירה של השימוש ב-Delta packets קטנים. צוואר הבקבוק האמיתי הוא **CPU/Memory** לכל `LiveMatch` בודד (tick loop, collision detection, cooldown tracking) — ולכן מספר ה-containers הנדרש נקבע לפי **Load Testing/Benchmarking בפועל** (כמה LiveMatch objects container בודד יכול להחזיק לפני שה-tick rate מתחיל לפגר), ולא לפי מגבלת רשת.

---

## 4. חלוקת תפקידי הקונטיינרים (Docker/Kubernetes Roles)

### נקודת המפתח: משך משחק קצר (30–90 שניות)
משך משחק ממוצע קצר, ביחס ל-5,000,000 משחקים פעילים בו-זמנית (10M שחקנים ÷ 2), מייצר **קצב תחלופה (churn rate) עצום** — מספר אדיר של אירועי יצירה/סיום `LiveMatch` בכל שנייה. המשמעות התכנונית:
- **container בודד לא נוצר ונהרס לכל משחק** — זו הייתה תקורת provisioning אדירה ביחס לחיי המשחק. במקום זאת, container ה-"Game Server" הוא **ארוך-טווח ויציב**, ומריץ בתוכו **הרבה** `LiveMatch` objects קצרי-חיים, כל אחד ב-**Strand** משלו (בדיוק כפי שכבר מתוכנן היום) — ה-Strand הוא שמאפשר ריבוי משחקים בו-זמנית בבטחה בתוך אותו process, ללא Mutexes מסובכים.
- `MatchFactory` צריך להיות קליל וזריז ביותר, שכן הוא פועל בתדירות גבוהה מאוד.

### טבלת תפקידים

| תפקיד (Container Role) | Stateful / Stateless | מאפייני Scaling | רכיבי קוד קיימים |
|---|---|---|---|
| **Gateway / TCP Entry** | Stateless | Scale מהיר לפי מספר חיבורים נכנסים | `TcpServer`, `TcpConnection` |
| **Matchmaking Service** | Stateful חלקית (waitingPool משותף ב-Redis) | Scale בינוני, לפי עומס בקשות שידוך | `MatchManager` (מנתק את ה-waitingPool ל-Redis) |
| **Game-Server (Worker)** | Stateful (LiveMatch בזיכרון + Snapshot חיצוני) | הכי הרבה instances; כל instance **ארוך-חיים**, מכיל הרבה LiveMatch קצרי-חיים | `LiveMatch`, `RealTimeArbiter`, `CollisionResolver`, Strand-per-match |
| **Session Registry** | In-memory מהיר (Redis) | Scale גבוה, עומס קריאה/כתיבה תדיר אך קליל | חדש: `match_id ↔ server address`, `player_id ↔ match_id` |
| **DB Service (Users)** | Stateful, Persistent Volume | Scale נמוך יחסית (הכי יציב) | `IUserRepository` → NoSQL (ראו סעיף 1) |

### Auto-Scaling (Kubernetes/K3s)
מספר ה-containers **אינו קבוע מראש**, אלא נגזר דינמית מ-Auto-Scaling לפי עומס אמיתי (K3s/Kubernetes HPA — Horizontal Pod Autoscaler), בהתבסס על מדדי CPU/מספר LiveMatch פעילים לכל instance. Health Checks מזהים containers שקרסו, ו-Self-Healing מרים instance חלופי אוטומטית — בשילוב עם ה-Session Registry ומנגנון ה-Reconnect הקיים, זהו הבסיס לזמינות גבוהה (אין "100% יציבות" מוחלטת בשום מערכת מבוזרת, אך יש Fault Tolerance מתוכנן).

---

## 5. תשתית תמיכה נוספת (Docker Config Reminders)

- חשיפת שני הפורטים במפורש (TCP + UDP), כפי שכבר מתועד בפרויקט:
```yaml
ports:
  - "8080:8080/tcp"
  - "8080:8080/udp"
```
- Docker Volume ל-DB המקומי (רלוונטי בשלב מעבר, עד השלמת המעבר ל-NoSQL מבוזר):
```yaml
volumes:
  - sqlite_data:/app/data
```
- Multi-stage Dockerfile לבניית תלויות (`boost`, `sqlite3`/NoSQL client, `libsodium`) בנפרד מתמונת ה-Runtime הרזה.

---

## 6. סיכום עקרונות התכנון

1. **הפרדת State לפי Lifetime**: משתמשים (ארוכי-טווח) → NoSQL מבוזר; משחקים חיים (קצרי-טווח, 30–90 שניות) → זיכרון + Snapshot תקופתי ל-Redis.
2. **הפרדת אחריות בין שכבות**: Gateway (חיבור) / Matchmaking (שידוך) / Game-Server (הרצת משחק) / Session Registry (ניתוב) / DB (משתמשים) — כל שכבה סקיילבילית בנפרד, לפי הצורך הספציפי שלה.
3. **ה-Strand-per-match וה-Repository Pattern שכבר תוכננו** מאפשרים את כל ההרחבה הזו **כמעט ללא שינוי בקוד ה-Engine/Match עצמו** — רק תוספת שכבות תשתית מסביב.
4. **Bandwidth אינו הבעיה** (הודות ל-Delta protocol קיים) — צוואר הבקבוק האמיתי הוא CPU/Memory לכל LiveMatch, ולכן מספר ה-containers נקבע על ידי Load Testing, ומנוהל דינמית על ידי Kubernetes Auto-Scaling ולא כמספר קבוע.
5. **אין "100% יציבות"**, אך יש תכנון מפורש ל-Fault Tolerance: Health Checks, Self-Healing, Session Registry, ו-Snapshot תקופתי המשמר גם timestamps יחסיים (Motion/Cooldown/Premove) ולא רק מיקום סטטי.

</div>
