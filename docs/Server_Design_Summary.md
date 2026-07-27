# סיכום תכנון שרת לסקאלה (Server Design for Scale)
## Kung-Fu Chess — Cloud & Kubernetes Design

מסמך זה מסכם את התכנון להרחבת ארכיטקטורת השרת לתמיכה בעומס גלובלי של **100,000,000 משתמשים רשומים** ו-**10,000,000 משתמשים פעילים בו-זמנית (Concurrent Users)**, תוך התמקדות בשכבות הענן והתשתית הסובבות את מנוע המשחק (Engine).

---

## 1. שכבת הנתונים (Data Layer) — Users DB

### הבעיה ב-SQLite בסקאלה
* SQLite מבוסס על קובץ בודד עם נעילה ברמת הקובץ (file-level locking), ואינו מותאם לכתיבה וקריאה בו-זמנית ממספר רב של מכונות/תהליכים מבוזרים.

### הדרישות מה-DB
* גישת Key-Value עיקרית (`user_id → {password_hash, username, elo}`).
* אין צורך ביחסים/JOINs מורכבים.
* תמיכה ב-Eventual Consistency (אין הכרח ב-ACID קשיח גלובלי עבור עדכוני Elo מיידיים).
* ביצועי קריאה/כתיבה גבוהים ו-Scale אופקי על פני Data Centers גלובליים.

### החלטה ארכיטקטונית
* מעבר מ-SQLite ל-**NoSQL Key-Value / Wide-Column DB** (כגון DynamoDB מנוהל או Cassandra/ScyllaDB בקוד פתוח).

### השפעה על הקוד
* בזכות ה-**Repository Pattern** (`IUserRepository`), השינוי מיושם באמצעות מחלקה חדשה (`DynamoUserRepository` / `CassandraUserRepository`) ללא צורך בשינוי לוגיקת המנוע או השרת הקיים.

---

## 2. שכבת ה-Matchmaking וה-Routing

### הבעיה
* החזקת `m_waitingPool` מקומי בזיכרון ה-RAM של כל instance מונעת משחקנים המחוברים לקונטיינרים נפרדים למצוא זה את זה.

### פתרון 3 השכבות
1. **Gateway / Load Balancer:** נקודת כניסה לחיבורי TCP/UDP.
2. **Matchmaking Service:** שירות Stateless עצמאי המנהל תור ממתינים (`waitingPool`) ב-**Redis משותף**, כך שכל ה-instances רואים את אותו מאגר שחקנים.
3. **Game-Server Instance:** קונטיינר המריץ אובייקטי `LiveMatch` פעילים, כאשר כל משחק מבודד ב-**Strand** משלו.
4. **Session Registry (Redis):** טבלת מיפוי מהירה בזיכרון המקשרת בין `match_id` לכתובת ה-Server, ובין `player_id` ל-`match_id` הפעיל שלו (משמש ל-Reconnect ו-Spectate).

### התאוששות מקריסה (Fault Tolerance)
* **Self-Healing:** זיהוי נפילת קונטיינר ע"י Kubernetes והרמת instance חלופי.
* **Snapshot תקופתי ל-Redis:** שמירת תמונת מצב (`GameSnapshot`) של משחקים חיים כל 5 שניות.
* **שמירת Timestamps יחסיים:** ה-Snapshot כולל זמני תפוגה והגעה יחסיים של מהלכים באוויר (`Motion`), Cooldowns ו-PremoveQueue, על מנת למנוע "קפיצות" או פגיעה בתזמון העדין של המשחק בעת שחזור.

---

## 3. עומס תעבורת רשת (Network Traffic Load)

### חישוב עומס ותעבורה
* **הודעות נכנסות:** 10M שחקנים פעילים × (מהלך אחד כל 2 שניות) = **5,000,000 הודעות/שנייה**.
* **תשדורת כוללת (כולל יריב/צופים):** כ-**10,000,000 הודעות/שנייה**.
* **גודל הודעה:** כ-25 בתים (בזכות שידור Delta בלבד).
* **רוחב פס כולל:** 10M × 25 Bytes = **250 MB/s ≈ 2 Gbps** גלובלי.

### מסקנה
* נפח תעבורה של 2 Gbps נמוך מאוד ביחס לתשתיות ענן. **רוחב הפס אינו צוואר הבקבוק**.
* צוואר הבקבוק האמיתי הוא משאבי **CPU ו-Memory** לכל `LiveMatch` (ניהול ה-Tick Loop, גילוי התנגשויות וצינון). כמות הקונטיינרים תיקבע לפי בדיקות עומסים (Load Testing) ולא לפי מגבלת רשת.

---

## 4. חלוקת תפקידי הקונטיינרים (Docker/Kubernetes)

### אופי השרת והמבנה
* מכיוון שאורך משחק ממוצע קצר (30–90 שניות), הרמת קונטיינר ייעודי לכל משחק מייצרת תקורה אדירה.
* **הפתרון:** קונטיינרי ה-Game Server הם **ארוכי-טווח (Long-lived Workers)**, ומריצים בתוכם מספר רב של משחקים קצרי-חיים במקביל ע"י שימוש ב-Boost.Asio Strands.

### טבלת תפקידים

| תפקיד (Container Role) | Stateful / Stateless | מאפייני Scaling | רכיבי קוד |
|---|---|---|---|
| **Gateway / TCP Entry** | Stateless | Scale לפי כמות חיבורים נכנסים | `TcpServer`, `TcpConnection` |
| **Matchmaking Service** | Stateful חלקית (Redis) | Scale לפי עומס בקשות זיווג | `MatchManager` |
| **Game-Server (Worker)** | Stateful (RAM + Redis Snapshots) | הנתח הגדול ביותר; קונטיינרים ארוכי-חיים | `LiveMatch`, Strands, `RealTimeArbiter` |
| **Session Registry** | In-memory (Redis) | Scale גבוה, קריאה/כתיבה מהירה | מיפוי ניתוב שחקנים ומשחקים |
| **DB Service (Users)** | Stateful | Scale יציב ונמוך יותר | `IUserRepository` (NoSQL) |

---

## 5. תשתית וקונפיגורציית Docker

* **חשיפת פורטים מקבילה:** חשיפה explicit של TCP ו-UDP באותו הפורט:
  ```yaml
  ports:
    - "8080:8080/tcp"
    - "8080:8080/udp"
  ```
* **Docker Volume:** שמירת Persistent Volume עבור מסד הנתונים.
* **Multi-stage Dockerfile:** הפרדת סביבת הבנייה (תלויות Boost, Libsodium, SQLite/NoSQL) מתמונת ה-Runtime הסופית.

---

## 6. עקרונות תכנון מרכזיים

1. **הפרדת State לפי אורך חיים:** נתוני משתמשים קבועים ב-NoSQL; מצב משחק חי ב-RAM עם Snapshot תקופתי ב-Redis.
2. **הפרדת אחריות (Decoupling):** חלוקה למיקרו-שירותים עצמאיים (Gateway, Matchmaker, Worker, Registry, DB).
3. **שימור מנוע המשחק:** הארכיטקטורה מאפשרת Scale-Out מלא ללא שינוי בלוגיקת ה-Engine.
4. **עמידות בתקלות (Fault Tolerance):** שילוב של Self-Healing, Session Registry ו-Snapshots המשמרים זמנים יחסיים מבטיח חוויית משחק רציפה גם בעת קריסת קונטיינר.