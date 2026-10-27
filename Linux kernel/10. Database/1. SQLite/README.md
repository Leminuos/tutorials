## 1. Nền tảng SQLite

### 1.1. SQLite là gì?

SQLite là một cơ sở dữ liệu nhúng (embedded database). Khác với MySQL hay PostgreSQL, SQLite không có server chạy riêng. Toàn bộ database chỉ là một file duy nhất trên ổ đĩa (ví dụ `data.db`). Ứng dụng link với thư viện SQLite (khoảng 600–900 KB) và đọc/ghi trực tiếp vào file đó.

```
MySQL / PostgreSQL                        

+-------+    TCP/socket    +--------+
|  App  | ---------------> | Server | ---> data files
+-------+                  +--------+
```

```
SQLite

+----------------------+
|  App                 |
|  +----------------+  |
|  | libsqlite3     |  |
|  +----------------+  |
+----------|-----------+
           v
       [data.db]
```

Đặc điểm của SQLite:
- **Serverless**: không cần cài đặt, cấu hình hay khởi động dịch vụ. Không có user, password, port.
- **Một file duy nhất**: toàn bộ database nằm trong một file. Backup hay chuyển dữ liệu chỉ là copy file (khi không có ai đang ghi). Khi đang ghi sẽ có thêm file phụ `-journal` hoặc `-wal`, `-shm`.
- **ACID**: transaction đảm bảo Atomicity, Consistency, Isolation, Durability, kể cả khi chương trình crash hoặc mất điện giữa chừng (với điều kiện cấu hình đúng, xem mục 4.1 và 5.4).
- **Nhỏ gọn**: thư viện khoảng 500 KB - 900 KB tùy kiến trúc và tùy chọn biên dịch, chạy được với vài trăm KB RAM.
- **Định dạng file ổn định**: file tạo từ năm 2004 (phiên bản 3.0) vẫn đọc được bằng bản mới nhất và file độc lập với kiến trúc CPU (big/little endian, 32/64 bit).
- **Public domain**: không ràng buộc license, dùng được cho sản phẩm thương mại mà không phải công bố source.

Vì sao SQLite phổ biến trong embedded:
- Thiết bị embedded thường chỉ có một ứng dụng truy cập dữ liệu, không cần mô hình client/server.
- Không tốn RAM và CPU cho một tiến trình server chạy nền.
- Thay thế được các file text/binary tự định nghĩa: có sẵn truy vấn, index, transaction, chống hỏng dữ liệu khi mất điện.
- Được tích hợp sẵn trong Yocto, Buildroot, Android và hầu hết các bản phân phối Linux.

Khi nào không nên dùng SQLite:
- Nhiều client truy cập qua mạng: SQLite không phải database server và không nên đặt file trên NFS/SMB vì cơ chế lock không tin cậy.
- Ghi đồng thời với tần suất rất cao từ nhiều tiến trình: SQLite chỉ cho phép một writer tại một thời điểm.
- Dữ liệu cực lớn (hàng TB) cần phân tán.

### 1.2. SQL cơ bản

Phần này dùng một ví dụ xuyên suốt: lưu dữ liệu từ các cảm biến. Có thể mở `sqlite3 demo.db` rồi gõ lần lượt các câu lệnh để thử, bật `.headers on` và `.mode column` để kết quả dễ đọc.

#### 1.2.1. Quy ước cú pháp chung

```sql
-- Comment một dòng
/* Comment
   nhiều dòng */

SELECT name, unit FROM sensor WHERE unit = 'C';
```

- Mỗi câu lệnh kết thúc bằng dấu `;`. Một câu lệnh có thể viết trên nhiều dòng.
- Từ khóa (`SELECT`, `FROM`, `WHERE`,...) không phân biệt hoa thường. Quy ước thường viết hoa từ khóa, viết thường tên bảng và tên cột cho dễ đọc.
- Tên bảng, tên cột cũng không phân biệt hoa thường: `Sensor` và `sensor` là một bảng.
- **Chuỗi** đặt trong dấu nháy đơn: `'temp0'`. Muốn có dấu nháy đơn trong chuỗi thì viết hai lần: `'it''s'`.
- **Tên** (bảng, cột) trùng từ khóa hoặc có khoảng trắng thì đặt trong dấu nháy kép: `"order"`, `"sensor name"`.
- `NULL` nghĩa là không có giá trị, khác với `0` hay chuỗi rỗng `''`.

:::warning Lưu ý
Không dùng dấu nháy kép cho chuỗi. Vì lý do tương thích, SQLite mặc định chấp nhận `"abc"` như một chuỗi nếu không có cột nào tên `abc`. Khi gõ sai tên cột, câu lệnh không báo lỗi mà âm thầm so sánh với chuỗi. Build với `SQLITE_DQS=0` (mục 2.3) để tắt hành vi này.
:::

Các toán tử thường dùng:

| Nhóm | Toán tử | Ví dụ |
| --- | --- | --- |
| Số học | `+ - * / %` | `value * 1.8 + 32`, `ts / 60` (chia số nguyên nếu cả hai vế là số nguyên) |
| So sánh | `= == != <> < <= > >=` | `unit = 'C'`, `value <> 0` |
| Logic | `AND OR NOT` | `value > 20 AND value < 30` |
| Nối chuỗi | `\|\|` | `name \|\| ' (' \|\| unit \|\| ')'` -> `temp0 (C)` |
| Khoảng | `BETWEEN a AND b` | `value BETWEEN 20 AND 30` (bao gồm cả 20 và 30) |
| Tập hợp | `IN (...)` | `sensor_id IN (1, 3, 5)` |
| Mẫu chuỗi | `LIKE`, `GLOB` | `name LIKE 'temp%'` (`%`: chuỗi bất kỳ, `_`: một ký tự, không phân biệt hoa thường với ASCII), `name GLOB 'temp*'` (phân biệt hoa thường) |
| Kiểm tra NULL | `IS NULL`, `IS NOT NULL` | `location IS NULL` |

:::warning Lưu ý
Mọi phép so sánh với `NULL` bằng `=`, `<>`, `<`,... đều cho kết quả `NULL` (không đúng cũng không sai), nên `WHERE location = NULL` không bao giờ trả về dòng nào. Phải dùng `location IS NULL`.
:::

#### 1.2.2. CREATE TABLE

Cú pháp tổng quát:

```sql
CREATE TABLE [IF NOT EXISTS] tên_bảng (
    tên_cột  kiểu  [ràng_buộc_cột ...],
    tên_cột  kiểu  [ràng_buộc_cột ...],
    ...
    [ràng_buộc_bảng ...]
) [STRICT] [, WITHOUT ROWID];
```

- `IF NOT EXISTS`: không báo lỗi nếu bảng đã tồn tại. Rất hay dùng trong code khởi tạo, chạy mỗi lần ứng dụng khởi động.
- `kiểu`: `INTEGER`, `REAL`, `TEXT`, `BLOB`,... (chi tiết ở mục 1.3).

Các ràng buộc cột (column constraint):

| Constraint | Ý nghĩa |
| --- | --- |
| `PRIMARY KEY` | Khóa chính: định danh duy nhất mỗi dòng. Mỗi bảng có tối đa một khóa chính |
| `NOT NULL` | Không cho phép giá trị `NULL` |
| `UNIQUE` | Không cho phép hai dòng có cùng giá trị ở cột này (`NULL` không tính là trùng) |
| `DEFAULT giá_trị` | Giá trị mặc định khi `INSERT` không truyền cột này. Có thể là hằng số hoặc biểu thức trong ngoặc: `DEFAULT (unixepoch())` |
| `CHECK (biểu_thức)` | Điều kiện phải đúng với mọi dòng, ví dụ `CHECK (value >= -40 AND value <= 125)` |
| `REFERENCES bảng(cột)` | Khóa ngoại: giá trị phải tồn tại trong cột của bảng kia |
| `COLLATE NOCASE` | So sánh chuỗi không phân biệt hoa thường ở cột này |

Ví dụ đầy đủ:

```sql
CREATE TABLE IF NOT EXISTS sensor (
    id       INTEGER PRIMARY KEY,
    name     TEXT    NOT NULL UNIQUE,
    unit     TEXT    NOT NULL,
    location TEXT,                                     -- được phép NULL
    enabled  INTEGER NOT NULL DEFAULT 1 CHECK (enabled IN (0, 1))
);

CREATE TABLE IF NOT EXISTS reading (
    id        INTEGER PRIMARY KEY,
    sensor_id INTEGER NOT NULL REFERENCES sensor(id) ON DELETE CASCADE,
    ts        INTEGER NOT NULL,                        -- Unix epoch (giây)
    value     REAL    NOT NULL
);
```

Giải thích:
- `id INTEGER PRIMARY KEY`: mỗi bảng thông thường có một cột ẩn `rowid` kiểu số nguyên 64 bit, là khóa của B-tree lưu dữ liệu. Cột khai báo đúng `INTEGER PRIMARY KEY` trở thành tên khác (alias) của `rowid`. Khi `INSERT` không truyền `id`, SQLite tự sinh giá trị bằng `rowid` lớn nhất hiện có cộng 1.
- `INTEGER PRIMARY KEY` không bảo đảm id không bao giờ dùng lại: xóa dòng có id lớn nhất thì id đó có thể được cấp lại. Muốn không bao giờ dùng lại thì viết `INTEGER PRIMARY KEY AUTOINCREMENT` (tốn thêm một lần ghi vào bảng `sqlite_sequence` mỗi lần insert, chỉ dùng khi thật sự cần).
- `ON DELETE CASCADE`: khi xóa một sensor, các reading của sensor đó tự động bị xóa theo. Các lựa chọn khác: `ON DELETE SET NULL`, `ON DELETE RESTRICT` (không cho xóa sensor khi còn reading).

:::warning Lưu ý
SQLite mặc định không kiểm tra khóa ngoại (`REFERENCES`, `ON DELETE CASCADE`). Phải bật cho từng kết nối bằng `PRAGMA foreign_keys = ON;` sau khi mở database.
:::

Ràng buộc bảng (table constraint) dùng khi ràng buộc liên quan tới nhiều cột:

```sql
CREATE TABLE IF NOT EXISTS calibration (
    sensor_id INTEGER NOT NULL,
    param     TEXT    NOT NULL,                -- 'offset', 'gain',...
    value     REAL    NOT NULL,
    PRIMARY KEY (sensor_id, param),            -- khóa chính gồm 2 cột
    FOREIGN KEY (sensor_id) REFERENCES sensor(id)
);
```

Mỗi cặp `(sensor_id, param)` là duy nhất: sensor 1 chỉ có một giá trị `offset`, nhưng sensor 1 và sensor 2 đều có `offset` riêng.

Hai tùy chọn cuối bảng:
- `STRICT`: kiểm tra kiểu dữ liệu chặt chẽ (mục 1.3).
- `WITHOUT ROWID`: không có cột `rowid` ẩn, dữ liệu được tổ chức theo `PRIMARY KEY`. Tiết kiệm dung lượng cho bảng có khóa chính không phải số nguyên, như `calibration` ở trên hoặc bảng cấu hình (mục 8.6).

**Xem cấu trúc bảng**

```sql
.tables                               -- trong sqlite3 CLI
.schema sensor
PRAGMA table_info(sensor);            -- dùng được cả trong code
SELECT name, sql FROM sqlite_schema WHERE type = 'table';
```

```
cid  name      type     notnull  dflt_value  pk
---  --------  -------  -------  ----------  --
0    id        INTEGER  0                    1
1    name      TEXT     1                    0
2    unit      TEXT     1                    0
3    location  TEXT     0                    0
4    enabled   INTEGER  1        1           0
```

**Sửa và xóa bảng**

```sql
ALTER TABLE sensor ADD COLUMN model TEXT;              -- thêm cột (luôn được thêm vào cuối)
ALTER TABLE sensor RENAME COLUMN model TO part_no;     -- đổi tên cột (từ 3.25)
ALTER TABLE sensor DROP COLUMN part_no;                -- xóa cột (từ 3.35)
ALTER TABLE sensor RENAME TO device_sensor;            -- đổi tên bảng
DROP TABLE IF EXISTS calibration;                      -- xóa bảng và toàn bộ dữ liệu
```

`ALTER TABLE` của SQLite hạn chế hơn MySQL/PostgreSQL: không đổi được kiểu cột hay thêm ràng buộc vào cột có sẵn. Khi cần, phải tạo bảng mới, copy dữ liệu sang (`INSERT INTO new SELECT ... FROM old`), xóa bảng cũ rồi đổi tên bảng mới, tất cả trong một transaction.

#### 1.2.3. INSERT

Cú pháp:

```sql
INSERT INTO tên_bảng (cột1, cột2, ...) VALUES (giá_trị1, giá_trị2, ...);
```

Số giá trị phải bằng số cột liệt kê, theo đúng thứ tự. Cột không liệt kê nhận giá trị `DEFAULT`, hoặc `NULL` nếu không có `DEFAULT`, hoặc tự sinh nếu là `INTEGER PRIMARY KEY`.

```sql
-- Dữ liệu mẫu dùng cho các mục sau
INSERT INTO sensor (name, unit, location) VALUES ('temp0', 'C', 'cabinet');
INSERT INTO sensor (name, unit, location) VALUES ('hum0',  '%', 'cabinet');
INSERT INTO sensor (name, unit)           VALUES ('temp1', 'C');      -- location = NULL

-- Nhiều dòng trong một câu lệnh
INSERT INTO reading (sensor_id, ts, value) VALUES
    (1, 1761465600, 25.4),
    (1, 1761465660, 25.6),
    (1, 1761465720, 26.1),
    (2, 1761465600, 61.0),
    (2, 1761465660, 60.2);
```

Có thể bỏ danh sách cột, khi đó phải truyền đủ giá trị cho mọi cột theo thứ tự khai báo: `INSERT INTO sensor VALUES (NULL, 'temp2', 'C', NULL, 1);`. Không nên viết như vậy trong code vì khi bảng thêm cột, câu lệnh sẽ lỗi.

Các dạng khác:

```sql
-- Lấy dữ liệu từ câu SELECT
INSERT INTO reading_archive (sensor_id, ts, value)
SELECT sensor_id, ts, value FROM reading WHERE ts < 1761400000;

-- Mọi cột nhận giá trị mặc định
INSERT INTO counter DEFAULT VALUES;

-- Trả về giá trị vừa chèn (từ 3.35)
INSERT INTO sensor (name, unit) VALUES ('press0', 'hPa') RETURNING id;

-- id của dòng vừa chèn bởi connection hiện tại
SELECT last_insert_rowid();
```

**Xử lý khi vi phạm ràng buộc**

Mặc định, vi phạm `UNIQUE`, `NOT NULL`, `CHECK`,... làm câu lệnh lỗi (`SQLITE_CONSTRAINT`) và không có gì được chèn:

```sql
INSERT INTO sensor (name, unit) VALUES ('temp0', 'C');
-- Runtime error: UNIQUE constraint failed: sensor.name (19)
```

Có thể chỉ định cách xử lý:

| Cú pháp | Khi trùng |
| --- | --- |
| `INSERT OR IGNORE INTO ...` | Bỏ qua dòng bị trùng, không báo lỗi |
| `INSERT OR REPLACE INTO ...` | Xóa dòng cũ rồi chèn dòng mới (dòng mới có `rowid` mới, cột không truyền bị đặt về mặc định, trigger `DELETE` và `ON DELETE CASCADE` có thể chạy) |
| `INSERT ... ON CONFLICT(cột) DO NOTHING` | Như `OR IGNORE`, nhưng chỉ với ràng buộc trên cột chỉ định |
| `INSERT ... ON CONFLICT(cột) DO UPDATE SET ...` | UPSERT: cập nhật dòng cũ (từ 3.24) |

```sql
-- UPSERT: chưa có thì thêm, có rồi thì cập nhật unit
INSERT INTO sensor (name, unit) VALUES ('temp0', 'K')
    ON CONFLICT(name) DO UPDATE SET unit = excluded.unit;
```

`excluded` là tên đặc biệt chỉ dòng **định chèn** (bị từ chối do trùng). `excluded.unit` là `'K'`, còn `unit` không có tiền tố là giá trị đang có trong bảng.

Nên dùng UPSERT thay vì `INSERT OR REPLACE` khi chỉ muốn cập nhật một vài cột: `REPLACE` xóa cả dòng nên làm mất các cột không truyền vào và có thể xóa theo dữ liệu liên quan qua `ON DELETE CASCADE`.

#### 1.2.4. SELECT

Cú pháp đầy đủ, các mệnh đề phải viết đúng thứ tự này:

```sql
SELECT [DISTINCT] biểu_thức [AS bí_danh], ...
FROM bảng
[JOIN bảng_khác ON điều_kiện]
[WHERE điều_kiện_lọc_dòng]
[GROUP BY cột, ...]
[HAVING điều_kiện_lọc_nhóm]
[ORDER BY cột [ASC|DESC], ...]
[LIMIT n [OFFSET m]];
```

Thứ tự viết khác với thứ tự SQLite xử lý logic. Hiểu thứ tự xử lý giúp biết vì sao `WHERE` không dùng được bí danh khai báo trong `SELECT`, hay vì sao lọc theo `AVG()` phải dùng `HAVING`:

```
1. FROM / JOIN    lấy dữ liệu từ các bảng, ghép bảng
       |
       v
2. WHERE          lọc từng dòng
       |
       v
3. GROUP BY       gom các dòng thành nhóm
       |
       v
4. HAVING         lọc từng nhóm (dùng được COUNT, AVG,...)
       |
       v
5. SELECT         tính các biểu thức, đặt bí danh
       |
       v
6. DISTINCT       loại dòng trùng
       |
       v
7. ORDER BY       sắp xếp (dùng được bí danh)
       |
       v
8. LIMIT/OFFSET   cắt lấy n dòng
```

**Chọn cột**

```sql
SELECT * FROM sensor;                              -- mọi cột (chỉ nên dùng khi debug)
SELECT name, unit FROM sensor;                     -- chỉ các cột cần thiết
SELECT name AS sensor_name,                        -- đặt bí danh cho cột kết quả
       value * 1.8 + 32 AS fahrenheit              -- biểu thức tính toán
FROM ...;
SELECT DISTINCT unit FROM sensor;                  -- các giá trị không trùng: 'C', '%'
```

Trong code nên liệt kê rõ các cột thay vì `SELECT *`: khi bảng thêm cột, thứ tự và số lượng cột của `SELECT *` thay đổi làm code đọc theo chỉ số cột bị sai.

**Lọc dòng với WHERE**

```sql
SELECT * FROM reading WHERE sensor_id = 1;
SELECT * FROM reading WHERE sensor_id = 1 AND value > 25.5;
SELECT * FROM reading WHERE ts BETWEEN 1761465600 AND 1761465700;
SELECT * FROM reading WHERE sensor_id IN (1, 2);
SELECT * FROM sensor  WHERE name LIKE 'temp%';
SELECT * FROM sensor  WHERE location IS NULL;
SELECT * FROM sensor  WHERE NOT (unit = 'C' OR unit = '%');
```

`AND` được ưu tiên hơn `OR`, nên `a OR b AND c` nghĩa là `a OR (b AND c)`. Khi kết hợp `AND` và `OR` luôn thêm ngoặc cho rõ ràng.

**Sắp xếp và giới hạn**

```sql
-- 2 mẫu mới nhất của sensor 1
SELECT ts, value FROM reading
WHERE sensor_id = 1
ORDER BY ts DESC
LIMIT 2;
```

```
ts          value
----------  -----
1761465720  26.1
1761465660  25.6
```

- `ASC` (mặc định): tăng dần. `DESC`: giảm dần.
- Sắp xếp nhiều cột: `ORDER BY sensor_id ASC, ts DESC` sắp theo `sensor_id`, các dòng cùng `sensor_id` sắp theo `ts` giảm dần.
- Không có `ORDER BY` thì **thứ tự kết quả không được bảo đảm**, kể cả khi chạy thử thấy đúng thứ tự chèn.
- `LIMIT 10 OFFSET 20`: bỏ 20 dòng đầu, lấy 10 dòng tiếp theo (phân trang). Với bảng lớn, `OFFSET` lớn sẽ chậm vì SQLite vẫn phải duyệt qua các dòng bị bỏ. Nên phân trang bằng điều kiện: `WHERE id > :last_id ORDER BY id LIMIT 10`.

**Hàm gộp (aggregate) và GROUP BY**

| Hàm | Ý nghĩa |
| --- | --- |
| `COUNT(*)` | Số dòng |
| `COUNT(cột)` | Số dòng có `cột` khác `NULL` |
| `SUM(cột)`, `AVG(cột)` | Tổng, trung bình (bỏ qua `NULL`) |
| `MIN(cột)`, `MAX(cột)` | Nhỏ nhất, lớn nhất |
| `group_concat(cột, ',')` | Nối các giá trị thành một chuỗi |

Không có `GROUP BY`, hàm gộp tính trên toàn bộ các dòng sau `WHERE` và trả về một dòng:

```sql
SELECT COUNT(*), AVG(value) FROM reading WHERE sensor_id = 1;
```

```
COUNT(*)  AVG(value)
--------  ----------
3         25.7
```

Có `GROUP BY`, các dòng có cùng giá trị ở cột nhóm được gom lại, hàm gộp tính riêng cho từng nhóm:

```sql
SELECT sensor_id,
       COUNT(*)             AS n,
       ROUND(AVG(value), 2) AS avg,
       MIN(value)           AS min,
       MAX(value)           AS max
FROM reading
GROUP BY sensor_id;
```

```
sensor_id  n  avg    min   max
---------  -  -----  ----  ----
1          3  25.7   25.4  26.1
2          2  60.6   60.2  61.0
```

Quy tắc: mỗi cột trong `SELECT` phải hoặc nằm trong `GROUP BY`, hoặc nằm trong một hàm gộp. SQLite không báo lỗi khi vi phạm mà trả về giá trị của một dòng bất kỳ trong nhóm, đây là nguồn gốc của kết quả sai khó phát hiện.

**WHERE và HAVING**

```sql
SELECT sensor_id, AVG(value) AS avg
FROM reading
WHERE ts >= 1761465600          -- lọc DÒNG trước khi gom nhóm
GROUP BY sensor_id
HAVING AVG(value) > 50;         -- lọc NHÓM sau khi đã tính AVG
```

```
sensor_id  avg
---------  ----
2          60.6
```

`WHERE` chạy trước `GROUP BY` nên không thể dùng `AVG()` trong `WHERE`. Điều kiện nào lọc được ở `WHERE` thì nên đặt ở `WHERE`, vì loại bớt dòng sớm giúp truy vấn nhanh hơn.

**Biểu thức CASE**

Tương tự `switch`/`if-else`, trả về giá trị theo điều kiện:

```sql
SELECT ts, value,
       CASE
           WHEN value >= 30 THEN 'HIGH'
           WHEN value <= 10 THEN 'LOW'
           ELSE 'NORMAL'
       END AS level
FROM reading WHERE sensor_id = 1;
```

**Một số hàm có sẵn hay dùng**

| Hàm | Ví dụ | Kết quả |
| --- | --- | --- |
| `round(x, n)` | `round(25.678, 1)` | `25.7` |
| `abs(x)` | `abs(-3)` | `3` |
| `length(s)` | `length('temp0')` | `5` |
| `upper(s)`, `lower(s)` | `upper('temp')` | `'TEMP'` |
| `substr(s, start, len)` | `substr('temp0', 1, 4)` | `'temp'` (chỉ số bắt đầu từ 1) |
| `replace(s, a, b)` | `replace('a-b', '-', '_')` | `'a_b'` |
| `trim(s)` | `trim('  x ')` | `'x'` |
| `coalesce(a, b, ...)` | `coalesce(location, 'unknown')` | Giá trị khác `NULL` đầu tiên |
| `ifnull(a, b)` | `ifnull(location, '-')` | Như `coalesce` với 2 tham số |
| `printf(fmt, ...)` | `printf('%.2f', 3.14159)` | `'3.14'` |
| `typeof(x)` | `typeof(1.5)` | `'real'` |
| `datetime(ts, 'unixepoch')` | `datetime(1761465600, 'unixepoch')` | `'2025-10-26 08:00:00'` |

**Truy vấn con (subquery) và CTE**

Một câu `SELECT` có thể nằm bên trong câu lệnh khác:

```sql
-- Các reading lớn hơn trung bình của chính sensor đó
SELECT * FROM reading r
WHERE value > (SELECT AVG(value) FROM reading WHERE sensor_id = r.sensor_id);

-- Các sensor đặt trong tủ
SELECT * FROM reading
WHERE sensor_id IN (SELECT id FROM sensor WHERE location = 'cabinet');
```

Với truy vấn phức tạp, `WITH` (Common Table Expression) đặt tên cho kết quả trung gian, giúp đọc dễ hơn lồng nhiều tầng:

```sql
WITH latest AS (
    SELECT sensor_id, MAX(ts) AS ts
    FROM reading
    GROUP BY sensor_id
)
SELECT s.name, r.ts, r.value              -- giá trị mới nhất của mỗi sensor
FROM latest l
JOIN reading r ON r.sensor_id = l.sensor_id AND r.ts = l.ts
JOIN sensor  s ON s.id = l.sensor_id;
```

#### 1.2.5. UPDATE

Cú pháp:

```sql
UPDATE tên_bảng
SET cột1 = biểu_thức1, cột2 = biểu_thức2, ...
[WHERE điều_kiện];
```

```sql
UPDATE sensor SET location = 'outdoor' WHERE name = 'hum0';

-- Cập nhật nhiều cột, dùng giá trị cũ để tính giá trị mới
UPDATE sensor SET enabled = 0, location = location || ' (disabled)' WHERE id = 3;

-- Hiệu chỉnh offset cho toàn bộ dữ liệu của sensor 1
UPDATE reading SET value = value - 0.5 WHERE sensor_id = 1;

-- Trả về các dòng đã sửa (từ 3.35)
UPDATE sensor SET enabled = 1 WHERE enabled = 0 RETURNING id, name;
```

Biểu thức bên phải dấu `=` dùng giá trị **trước khi** cập nhật: `SET a = b, b = a` sẽ hoán đổi hai cột.

Số dòng bị ảnh hưởng: `SELECT changes();` trong SQL hoặc `sqlite3_changes(db)` trong C. Giá trị `0` nghĩa là `WHERE` không khớp dòng nào, câu lệnh vẫn thành công.

#### 1.2.6. DELETE

Cú pháp:

```sql
DELETE FROM tên_bảng [WHERE điều_kiện];
```

```sql
DELETE FROM reading WHERE ts < 1761400000;                   -- xóa dữ liệu cũ
DELETE FROM reading WHERE sensor_id NOT IN (SELECT id FROM sensor);  -- dữ liệu mồ côi
DELETE FROM reading;                                         -- xóa TOÀN BỘ bảng
```

:::warning Lưu ý
`UPDATE` và `DELETE` thiếu `WHERE` sẽ áp dụng lên **mọi dòng** của bảng. Trước khi chạy, nên thử điều kiện bằng `SELECT COUNT(*) FROM ... WHERE ...` để biết số dòng sẽ bị ảnh hưởng. Khi thao tác tay trên thiết bị, mở `BEGIN;` trước để còn `ROLLBACK;` nếu sai.
:::

`DELETE` chỉ đánh dấu page là trống để dùng lại, file database không nhỏ lại (mục 8.2).

#### 1.2.7. JOIN

JOIN ghép dòng của hai bảng dựa trên một điều kiện, thường là khóa ngoại. Với dữ liệu mẫu:

```
sensor                        reading
+----+-------+------+         +----+-----------+------------+-------+
| id | name  | unit |         | id | sensor_id | ts         | value |
+----+-------+------+         +----+-----------+------------+-------+
| 1  | temp0 | C    |         | 1  | 1         | 1761465600 | 25.4  |
| 2  | hum0  | %    |         | 2  | 1         | 1761465660 | 25.6  |
| 3  | temp1 | C    |         | 3  | 1         | 1761465720 | 26.1  |
+----+-------+------+         | 4  | 2         | 1761465600 | 61.0  |
                              | 5  | 2         | 1761465660 | 60.2  |
                              +----+-----------+------------+-------+
```

Sensor `temp1` (id 3) chưa có reading nào.

**INNER JOIN** (viết `JOIN` là đủ): chỉ giữ các cặp dòng thỏa điều kiện ở cả hai bảng.

```sql
SELECT s.name, s.unit, r.ts, r.value
FROM reading AS r
JOIN sensor  AS s ON s.id = r.sensor_id
WHERE s.unit = 'C';
```

```
name   unit  ts          value
-----  ----  ----------  -----
temp0  C     1761465600  25.4
temp0  C     1761465660  25.6
temp0  C     1761465720  26.1
```

- `AS r`, `AS s`: bí danh bảng cho câu lệnh ngắn gọn (có thể bỏ chữ `AS`). Khi hai bảng có cột trùng tên (`id`), phải ghi rõ `s.id` hay `r.id`.
- `temp1` không xuất hiện vì không có reading nào khớp.

**LEFT JOIN**: giữ **mọi dòng của bảng bên trái**, dòng không có cặp ở bảng bên phải thì các cột bên phải là `NULL`.

```sql
SELECT s.name, COUNT(r.id) AS n, MAX(r.ts) AS last_ts
FROM sensor s
LEFT JOIN reading r ON r.sensor_id = s.id
GROUP BY s.id;
```

```
name   n  last_ts
-----  -  ----------
temp0  3  1761465720
hum0   2  1761465660
temp1  0
```

Dùng `COUNT(r.id)` thay vì `COUNT(*)`: với `temp1`, dòng kết quả vẫn tồn tại (với `r.id = NULL`) nên `COUNT(*)` cho `1`, còn `COUNT(r.id)` cho `0` đúng ý nghĩa.

Tìm sensor chưa có dữ liệu nào:

```sql
SELECT s.name FROM sensor s
LEFT JOIN reading r ON r.sensor_id = s.id
WHERE r.id IS NULL;              -- -> temp1
```

:::tip Bẫy thường gặp với LEFT JOIN
Điều kiện trên bảng bên phải đặt ở `ON` hay `WHERE` cho kết quả khác nhau:
- `LEFT JOIN reading r ON r.sensor_id = s.id AND r.ts > 1761465650`: vẫn giữ mọi sensor, chỉ ghép với các reading thỏa điều kiện thời gian.
- `LEFT JOIN reading r ON r.sensor_id = s.id WHERE r.ts > 1761465650`: `WHERE` loại các dòng có `r.ts` là `NULL`, sensor không có reading bị mất, kết quả giống hệt `INNER JOIN`.
:::

Các loại JOIN khác:

| Loại | Kết quả |
| --- | --- |
| `CROSS JOIN` | Mọi tổ hợp dòng của hai bảng (3 sensor x 5 reading = 15 dòng). Hiếm khi cần |
| `RIGHT JOIN` | Ngược với `LEFT JOIN`: giữ mọi dòng bảng bên phải (từ 3.39) |
| `FULL JOIN` | Giữ mọi dòng của cả hai bảng (từ 3.39) |
| Self join | Ghép một bảng với chính nó, ví dụ so sánh mẫu với mẫu liền trước |

#### 1.2.8. INDEX

Cú pháp:

```sql
CREATE [UNIQUE] INDEX [IF NOT EXISTS] tên_index
ON tên_bảng (cột1 [ASC|DESC], cột2, ...)
[WHERE điều_kiện];
```

```sql
-- Index thường, phục vụ truy vấn "dữ liệu của sensor X trong khoảng thời gian"
CREATE INDEX IF NOT EXISTS idx_reading_sensor_ts ON reading(sensor_id, ts);

-- UNIQUE index: vừa tăng tốc tìm kiếm, vừa là ràng buộc không trùng
CREATE UNIQUE INDEX IF NOT EXISTS idx_reading_uniq ON reading(sensor_id, ts);

-- Partial index: chỉ index các dòng thỏa điều kiện (mục 8.4)
CREATE INDEX IF NOT EXISTS idx_reading_unsent ON reading(id) WHERE sent = 0;

-- Index trên biểu thức
CREATE INDEX IF NOT EXISTS idx_sensor_lower_name ON sensor(lower(name));

-- Xem và xóa index
PRAGMA index_list(reading);
DROP INDEX IF EXISTS idx_reading_uniq;
```

Có thể hình dung index như mục lục cuối sách: một danh sách đã sắp xếp theo `(sensor_id, ts)`, mỗi mục trỏ về `rowid` của dòng dữ liệu. Không có index, tìm reading của sensor 1 phải đọc toàn bộ bảng. Có index, SQLite tìm nhị phân trong B-tree của index rồi đọc đúng các dòng cần.

Đổi lại, mỗi index là một B-tree riêng phải được cập nhật mỗi khi `INSERT`/`UPDATE`/`DELETE`, làm ghi chậm hơn và tăng số lần ghi xuống flash. Chỉ tạo index cho các truy vấn thật sự dùng, kiểm tra bằng `EXPLAIN QUERY PLAN` (mục 4.4).

`PRIMARY KEY` và `UNIQUE` tự động tạo index ngầm, không cần tạo thêm (riêng `INTEGER PRIMARY KEY` chính là khóa của bảng, không tốn index riêng).

#### 1.2.9. Transaction

Transaction gom nhiều câu lệnh thành một khối: hoặc tất cả được áp dụng, hoặc không có câu nào được áp dụng.

```sql
BEGIN;
INSERT INTO reading (sensor_id, ts, value) VALUES (1, 1761465780, 25.9);
INSERT INTO reading (sensor_id, ts, value) VALUES (2, 1761465780, 60.8);
COMMIT;                -- ghi vĩnh viễn cả hai dòng
```

```sql
BEGIN;
DELETE FROM reading WHERE sensor_id = 2;
SELECT COUNT(*) FROM reading;     -- trong transaction đã thấy kết quả xóa
ROLLBACK;                         -- hủy: dữ liệu quay về như trước BEGIN
```

- Không có `BEGIN`, mỗi câu lệnh tự động là một transaction riêng (autocommit).
- Các connection khác không thấy thay đổi cho đến khi `COMMIT`.
- Chương trình crash hoặc mất điện trước `COMMIT` thì như đã `ROLLBACK`.

Ngoài `BEGIN` còn có `BEGIN IMMEDIATE` và `BEGIN EXCLUSIVE`, khác nhau ở thời điểm lấy lock ghi (mục 4.3). Cơ chế bên trong của transaction ở mục 4.1.

`SAVEPOINT` tạo điểm khôi phục bên trong transaction, hủy một phần mà không hủy cả transaction:

```sql
BEGIN;
INSERT INTO sensor (name, unit) VALUES ('temp2', 'C');
SAVEPOINT sp1;
INSERT INTO sensor (name, unit) VALUES ('temp3', 'C');
ROLLBACK TO sp1;       -- chỉ hủy temp3
RELEASE sp1;
COMMIT;                -- temp2 được ghi
```

#### 1.2.10. VIEW và TRIGGER

**VIEW** là một câu `SELECT` được đặt tên, dùng như một bảng chỉ đọc. Không lưu dữ liệu riêng, mỗi lần truy vấn view là chạy lại câu `SELECT`.

```sql
CREATE VIEW IF NOT EXISTS v_reading AS
SELECT s.name, s.unit, datetime(r.ts, 'unixepoch') AS time, r.value
FROM reading r JOIN sensor s ON s.id = r.sensor_id;

SELECT * FROM v_reading WHERE name = 'temp0' ORDER BY time DESC LIMIT 5;
```

View giúp web UI hoặc script debug truy vấn đơn giản mà không phải viết lại JOIN.

**TRIGGER** là đoạn SQL tự động chạy khi có `INSERT`, `UPDATE` hoặc `DELETE` trên một bảng:

```sql
CREATE TRIGGER IF NOT EXISTS trg_sensor_disable
AFTER UPDATE OF enabled ON sensor
WHEN NEW.enabled = 0
BEGIN
    INSERT INTO event_log (ts, msg)
    VALUES (unixepoch(), 'sensor ' || NEW.name || ' disabled');
END;
```

- `BEFORE` / `AFTER`: chạy trước hay sau thao tác.
- `NEW.cột`: giá trị mới (trong `INSERT`, `UPDATE`). `OLD.cột`: giá trị cũ (trong `UPDATE`, `DELETE`).
- `WHEN`: chỉ chạy khi điều kiện đúng.

Trigger chạy trong cùng transaction với câu lệnh gây ra nó. Lưu ý mỗi trigger làm tăng thêm số lần ghi, không nên lạm dụng trên bảng ghi tần suất cao. Mục 7.2 dùng trigger để cài ring buffer.

### 1.3. Type affinity và dynamic typing

Đây là điểm SQLite khác biệt lớn nhất so với MySQL/PostgreSQL và là nguồn gốc của nhiều bug khó tìm.

Trong MySQL/PostgreSQL, kiểu dữ liệu gắn với cột: cột `INTEGER` chỉ chứa số nguyên. Trong SQLite, kiểu dữ liệu gắn với từng giá trị. Kiểu khai báo của cột chỉ là gợi ý (type affinity), một cột vẫn có thể chứa giá trị thuộc kiểu khác.

SQLite chỉ có 5 storage class:

| Storage class | Ý nghĩa |
| --- | --- |
| `NULL` | Giá trị rỗng |
| `INTEGER` | Số nguyên có dấu, lưu bằng 0, 1, 2, 3, 4, 6 hoặc 8 byte tùy độ lớn |
| `REAL` | Số thực IEEE 754 8 byte |
| `TEXT` | Chuỗi UTF-8 hoặc UTF-16 |
| `BLOB` | Dữ liệu nhị phân, lưu nguyên dạng |

Không có kiểu `BOOLEAN` (dùng `INTEGER` 0/1) và không có kiểu `DATETIME` (dùng `INTEGER`, `REAL` hoặc `TEXT`, xem mục 8.1).

**Quy tắc xác định affinity từ tên kiểu khai báo** (xét theo thứ tự):

| Tên kiểu khai báo chứa | Affinity | Ví dụ |
| --- | --- | --- |
| `INT` | INTEGER | `INT`, `INTEGER`, `BIGINT`, `UNSIGNED BIG INT` |
| `CHAR`, `CLOB`, `TEXT` | TEXT | `VARCHAR(255)`, `TEXT`, `NCHAR(10)` |
| `BLOB` hoặc không khai báo kiểu | BLOB | `BLOB`, `data` (không có kiểu) |
| `REAL`, `FLOA`, `DOUB` | REAL | `REAL`, `DOUBLE`, `FLOAT` |
| Còn lại | NUMERIC | `NUMERIC`, `DECIMAL(10,5)`, `BOOLEAN`, `DATE`, `DATETIME` |

Một số hệ quả bất ngờ:
- `VARCHAR(10)` không giới hạn độ dài, chuỗi 1000 ký tự vẫn lưu được.
- `STRING` không chứa từ khóa nào ở trên nên có affinity NUMERIC: `'0123'` sẽ bị chuyển thành số `123`.
- `FLOATING POINT` chứa `INT` (trong "POINT") nên có affinity INTEGER.

**Khi insert, SQLite cố chuyển giá trị sang affinity của cột, nếu chuyển được mà không mất thông tin:**

```sql
CREATE TABLE t (i INTEGER, s TEXT);

INSERT INTO t VALUES ('42',  42);     -- i = 42 (INTEGER), s = '42' (TEXT)
INSERT INTO t VALUES ('abc', 1.5);    -- i = 'abc' (TEXT!), s = '1.5' (TEXT)
INSERT INTO t VALUES ('4.0', NULL);   -- i = 4 (INTEGER)

SELECT i, typeof(i), s, typeof(s) FROM t;
```

```
i    typeof(i)  s    typeof(s)
---  ---------  ---  ---------
42   integer    42   text
abc  text       1.5  text
4    integer         null
```

Cột `INTEGER` vẫn nhận chuỗi `'abc'` mà không báo lỗi.

**Bug điển hình: so sánh số với chuỗi**

```sql
CREATE TABLE cfg (key TEXT, value TEXT);
INSERT INTO cfg VALUES ('threshold', 100);    -- lưu thành '100' (TEXT)

SELECT * FROM cfg WHERE value > 99;                    -- KHÔNG có kết quả
SELECT * FROM cfg WHERE CAST(value AS INTEGER) > 99;   -- có kết quả
```

Khi so sánh, nếu một vế là cột có affinity TEXT và vế kia là số, số `99` bị chuyển thành chuỗi `'99'` rồi so sánh theo thứ tự chuỗi: `'100' < '99'` vì ký tự `'1'` nhỏ hơn `'9'`. Nếu cột `value` khai báo `INTEGER` thì câu đầu cho kết quả đúng. Kết quả phụ thuộc vào kiểu của cả hai vế, rất dễ sai khi code C bind giá trị bằng `sqlite3_bind_text` thay vì `sqlite3_bind_int`.

**Giải pháp: bảng STRICT** (từ bản 3.37.0, 2021)

```sql
CREATE TABLE reading (
    id        INTEGER PRIMARY KEY,
    sensor_id INTEGER NOT NULL,
    ts        INTEGER NOT NULL,
    value     REAL    NOT NULL,
    extra     ANY                 -- cột chấp nhận mọi kiểu
) STRICT;

INSERT INTO reading (sensor_id, ts, value) VALUES (1, 'abc', 1.0);
-- Runtime error: cannot store TEXT value in INTEGER column reading.ts
```

Bảng `STRICT` chỉ chấp nhận các kiểu `INT`, `INTEGER`, `REAL`, `TEXT`, `BLOB`, `ANY` và báo lỗi `SQLITE_CONSTRAINT_DATATYPE` khi giá trị không chuyển được về đúng kiểu. Với project mới, nên dùng `STRICT` nếu phiên bản SQLite trên thiết bị đủ mới (kiểm tra bằng `SELECT sqlite_version();`).

:::tip Mẹo
Với bảng không STRICT, có thể ép kiểu bằng `CHECK`: `ts INTEGER NOT NULL CHECK (typeof(ts) = 'integer')`.
:::

### 1.4. Công cụ

**sqlite3 CLI**

```bash
sudo apt install sqlite3
sqlite3 data.db
```

```
sqlite> .tables                      -- liệt kê bảng
sqlite> .schema reading              -- xem câu lệnh CREATE của bảng
sqlite> .headers on
sqlite> .mode column                 -- hiển thị dạng cột
sqlite> .timer on                    -- đo thời gian chạy mỗi câu lệnh
sqlite> .eqp on                      -- tự in query plan trước mỗi SELECT
sqlite> SELECT * FROM reading LIMIT 5;
sqlite> .dump                        -- xuất toàn bộ database thành SQL
sqlite> .backup /tmp/backup.db       -- backup an toàn khi database đang được dùng
sqlite> .quit
```

Chạy không tương tác, tiện dùng trong script:

```bash
sqlite3 data.db "SELECT COUNT(*) FROM reading;"
sqlite3 -csv -header data.db "SELECT * FROM reading;" > reading.csv
sqlite3 data.db < schema.sql
```

**DB Browser for SQLite**

Giao diện đồ họa để xem bảng, sửa dữ liệu, chạy truy vấn và xem schema. Cài bằng `sudo apt install sqlitebrowser`.

Quy trình debug database trên thiết bị:

```bash
# Trên thiết bị: tạo bản sao nhất quán (gộp cả nội dung trong file -wal)
sqlite3 /var/lib/app/data.db ".backup /tmp/data_copy.db"

# Trên PC
scp root@192.168.7.2:/tmp/data_copy.db .
sqlitebrowser data_copy.db
```

:::warning Lưu ý
Không copy trực tiếp file `.db` khi ứng dụng đang ghi. Ở chế độ WAL, dữ liệu mới nhất có thể vẫn nằm trong file `-wal`, copy riêng file `.db` sẽ thiếu dữ liệu hoặc nhận được bản không nhất quán. Dùng `.backup` hoặc `VACUUM INTO '/tmp/copy.db'`.
:::

## 2. Tích hợp vào hệ thống Embedded Linux

### 2.1. Cross-compile bằng bản amalgamation

SQLite được phát hành dưới dạng **amalgamation**: toàn bộ source gộp thành một file `sqlite3.c` (khoảng 9 MB, 250.000 dòng) cùng header `sqlite3.h`. Cách này giúp compiler tối ưu tốt hơn và rất dễ đưa vào project.

```bash
# Tải bản amalgamation tại https://sqlite.org/download.html
wget https://sqlite.org/2025/sqlite-amalgamation-3500400.zip
unzip sqlite-amalgamation-3500400.zip
cd sqlite-amalgamation-3500400
ls
# shell.c  sqlite3.c  sqlite3.h  sqlite3ext.h
```

Build thư viện static và shared cho ARM:

```bash
CC=arm-linux-gnueabihf-gcc
CFLAGS="-Os -DSQLITE_THREADSAFE=1 -DSQLITE_DQS=0 -DSQLITE_DEFAULT_MEMSTATUS=0 -DSQLITE_OMIT_LOAD_EXTENSION"

# Thư viện static
$CC $CFLAGS -c sqlite3.c -o sqlite3.o
arm-linux-gnueabihf-ar rcs libsqlite3.a sqlite3.o

# Thư viện shared
$CC $CFLAGS -fPIC -shared sqlite3.c -o libsqlite3.so -lpthread -lm

# Công cụ CLI để debug trên thiết bị
$CC $CFLAGS shell.c sqlite3.c -o sqlite3 -lpthread -lm
```

Link ứng dụng:

```bash
arm-linux-gnueabihf-gcc -Os main.c sqlite3.c -o app -lpthread -lm
# hoặc
arm-linux-gnueabihf-gcc -Os main.c -o app -L. -lsqlite3 -lpthread -lm
```

- `-lpthread`: cần khi `SQLITE_THREADSAFE` khác 0.
- `-ldl`: cần nếu không có `SQLITE_OMIT_LOAD_EXTENSION` (tính năng nạp extension `.so` lúc runtime).
- `-lm`: một số hàm toán học.

Với CMake, chỉ cần thêm `sqlite3.c` như một source thông thường:

```cmake
add_library(sqlite3 STATIC third_party/sqlite/sqlite3.c)
target_include_directories(sqlite3 PUBLIC third_party/sqlite)
target_compile_definitions(sqlite3 PRIVATE
    SQLITE_THREADSAFE=1
    SQLITE_DQS=0
    SQLITE_DEFAULT_MEMSTATUS=0
    SQLITE_OMIT_LOAD_EXTENSION)
find_package(Threads REQUIRED)
target_link_libraries(sqlite3 PUBLIC Threads::Threads m)

add_executable(app main.c)
target_link_libraries(app PRIVATE sqlite3)
```

Kiểm tra kết quả:

```bash
file app
# app: ELF 32-bit LSB executable, ARM, EABI5 version 1 (SYSV), dynamically linked, ...
```

### 2.2. Build system

**Yocto**

Recipe `sqlite3` có sẵn trong `openembedded-core` (`meta/recipes-support/sqlite/sqlite3_*.bb`).

Thêm vào image (trong `local.conf` hoặc recipe image):

```bitbake
IMAGE_INSTALL:append = " sqlite3"
```

Ứng dụng dùng thư viện sqlite thì khai báo trong recipe của ứng dụng:

```bitbake
DEPENDS += "sqlite3"
```

Sau đó dùng `pkg-config --cflags --libs sqlite3` hoặc `find_package(SQLite3)` trong CMake. Header và thư viện được đưa vào sysroot tự động.

Tùy biến tùy chọn biên dịch bằng một `bbappend` trong layer riêng:

```bitbake
# meta-mylayer/recipes-support/sqlite/sqlite3_%.bbappend
CFLAGS:append = " -DSQLITE_DEFAULT_WAL_SYNCHRONOUS=1 -DSQLITE_DEFAULT_MEMSTATUS=0"

# Bật/tắt tính năng có sẵn trong recipe
PACKAGECONFIG:remove = "readline"
```

Xem các `PACKAGECONFIG` mà recipe hỗ trợ:

```bash
bitbake -e sqlite3 | grep ^PACKAGECONFIG
```

**Buildroot**

Bật trong `make menuconfig`: `Target packages` -> `Libraries` -> `Database` -> `sqlite`.

```
BR2_PACKAGE_SQLITE=y
BR2_PACKAGE_SQLITE_STAT4=y              # thống kê tốt hơn cho query planner
BR2_PACKAGE_SQLITE_ENABLE_FTS3=y        # full text search
BR2_PACKAGE_SQLITE_ENABLE_UNLOCK_NOTIFY=y
BR2_PACKAGE_SQLITE_SECURE_DELETE=y      # ghi đè 0 lên dữ liệu bị xóa
BR2_PACKAGE_SQLITE_NO_SYNC=y            # tắt fsync, KHÔNG nên bật trên sản phẩm thật
```

Package của ứng dụng khai báo phụ thuộc:

```makefile
# package/myapp/myapp.mk
MYAPP_DEPENDENCIES = sqlite
```

```
# package/myapp/Config.in
config BR2_PACKAGE_MYAPP
	bool "myapp"
	select BR2_PACKAGE_SQLITE
```

Truyền thêm `CFLAGS` cho sqlite có thể sửa trực tiếp `package/sqlite/sqlite.mk` (biến `SQLITE_CFLAGS`) hoặc dùng `BR2_TARGET_OPTIMIZATION`, nhưng cách gọn hơn khi cần tùy biến nhiều là đưa amalgamation vào source của ứng dụng như mục 2.1.

### 2.3. Tùy chọn compile-time

Các tùy chọn được truyền qua `-D` khi biên dịch `sqlite3.c`.

**Tùy chọn quan trọng**

| Tùy chọn | Giá trị | Ý nghĩa |
| --- | --- | --- |
| `SQLITE_THREADSAFE` | `0` | Single-thread: bỏ toàn bộ mutex, nhỏ và nhanh nhất, chỉ dùng khi chắc chắn chỉ một thread truy cập SQLite |
| | `1` (mặc định) | Serialized: an toàn khi nhiều thread dùng chung một connection |
| | `2` | Multi-thread: nhiều thread được dùng SQLite, nhưng mỗi connection chỉ được dùng bởi một thread tại một thời điểm |
| `SQLITE_DEFAULT_CACHE_SIZE` | `-2000` (mặc định) | Kích thước page cache mỗi connection. Số âm là KiB (`-2000` = khoảng 2 MB), số dương là số page |
| `SQLITE_TEMP_STORE` | `0` | Luôn dùng file cho bảng tạm, index tạm (khi sort, GROUP BY,...) |
| | `1` (mặc định) | Mặc định dùng file, có thể đổi bằng `PRAGMA temp_store` |
| | `2` | Mặc định dùng RAM, có thể đổi bằng `PRAGMA temp_store` |
| | `3` | Luôn dùng RAM |
| `SQLITE_DEFAULT_WAL_SYNCHRONOUS` | `1` | Khi ở chế độ WAL, mặc định dùng `synchronous=NORMAL` thay vì `FULL` |
| `SQLITE_DEFAULT_MEMSTATUS` | `0` | Tắt thống kê bộ nhớ, giảm overhead cho mỗi lần cấp phát. Khi tắt thì `sqlite3_memory_used()` không còn chính xác |
| `SQLITE_DQS` | `0` | Không chấp nhận chuỗi trong dấu nháy kép (`"abc"`), tránh bug viết sai tên cột mà không báo lỗi |

**Nhóm `SQLITE_OMIT_*`** loại bỏ tính năng để giảm kích thước:

| Tùy chọn | Loại bỏ |
| --- | --- |
| `SQLITE_OMIT_LOAD_EXTENSION` | Nạp extension lúc runtime (bỏ phụ thuộc `libdl`) |
| `SQLITE_OMIT_DEPRECATED` | Các API cũ không còn khuyến khích dùng |
| `SQLITE_OMIT_SHARED_CACHE` | Shared cache mode (hầu như không ai cần), giúp code nhanh hơn |
| `SQLITE_OMIT_PROGRESS_CALLBACK` | `sqlite3_progress_handler()` |
| `SQLITE_OMIT_AUTOINIT` | Tự gọi `sqlite3_initialize()`; khi bật phải tự gọi trước mọi API khác |
| `SQLITE_OMIT_JSON` | Các hàm JSON |
| `SQLITE_OMIT_TRACE` | `sqlite3_trace_v2()` |
| `SQLITE_OMIT_UTF16` | Các API UTF-16 |

:::warning Lưu ý
Theo tài liệu chính thức, nhiều tùy chọn `SQLITE_OMIT_*` **không hoạt động đúng với bản amalgamation** đã sinh sẵn, vì chúng ảnh hưởng tới parser được sinh ra lúc build. Muốn dùng đầy đủ nhóm này phải build từ source gốc (canonical source) và truyền tùy chọn khi chạy `make sqlite3.c`. Các tùy chọn trong bảng trên và các tùy chọn không phải `OMIT` thì dùng được với amalgamation.
:::

Bộ tùy chọn khuyến nghị cho thiết bị embedded:

```bash
-Os \
-DSQLITE_THREADSAFE=1 \
-DSQLITE_DQS=0 \
-DSQLITE_DEFAULT_MEMSTATUS=0 \
-DSQLITE_DEFAULT_WAL_SYNCHRONOUS=1 \
-DSQLITE_LIKE_DOESNT_MATCH_BLOBS \
-DSQLITE_MAX_EXPR_DEPTH=0 \
-DSQLITE_OMIT_DEPRECATED \
-DSQLITE_OMIT_PROGRESS_CALLBACK \
-DSQLITE_OMIT_SHARED_CACHE \
-DSQLITE_OMIT_LOAD_EXTENSION \
-DSQLITE_USE_ALLOCA
```

### 2.4. Đo footprint

**Dung lượng binary**

```bash
arm-linux-gnueabihf-size sqlite3.o
#    text    data     bss     dec     hex filename
#  612340    9804     716  622860   9810c sqlite3.o

arm-linux-gnueabihf-strip libsqlite3.so
ls -lh libsqlite3.so
```

`text` là code và hằng số (nằm trên flash), `data` + `bss` là RAM tĩnh. So sánh kết quả giữa `-O2` và `-Os`, giữa các bộ tùy chọn khác nhau để chọn cấu hình phù hợp.

**RAM tiêu thụ**

SQLite cung cấp API thống kê bộ nhớ (cần `SQLITE_DEFAULT_MEMSTATUS=1`, là mặc định):

```c
printf("SQLite memory used: %lld bytes\n", sqlite3_memory_used());
printf("SQLite memory peak: %lld bytes\n", sqlite3_memory_highwater(0));

int cur, hi;
sqlite3_db_status(db, SQLITE_DBSTATUS_CACHE_USED, &cur, &hi, 0);
printf("Page cache: %d bytes\n", cur);
```

Đo từ phía hệ điều hành:

```bash
grep -E "VmRSS|VmHWM" /proc/$(pidof app)/status
# VmHWM:     3120 kB      <- đỉnh RSS
# VmRSS:     2876 kB
```

Trong CLI, lệnh `.stats on` in thống kê bộ nhớ sau mỗi câu lệnh.

Giới hạn RAM SQLite được dùng:

```c
sqlite3_soft_heap_limit64(512 * 1024);  /* cố giữ dưới 512 KB, giải phóng cache khi vượt */
sqlite3_hard_heap_limit64(1024 * 1024); /* vượt 1 MB thì cấp phát thất bại -> SQLITE_NOMEM */
```

Phần RAM lớn nhất thường là page cache. Giảm bằng `PRAGMA cache_size` (mục 4.5), đổi lại truy vấn chậm hơn vì phải đọc từ flash nhiều hơn.

## 3. Lập trình với SQLite API

### 3.1. Luồng hoạt động

```
sqlite3_open_v2()                    mở/tạo database -> sqlite3 *db
        |
        v
sqlite3_prepare_v2()                 biên dịch SQL -> sqlite3_stmt *stmt
        |
        v
sqlite3_bind_*()  <-----------+      gán giá trị cho tham số ?
        |                     |
        v                     |
sqlite3_step()  <---+         |      thực thi: SQLITE_ROW / SQLITE_DONE / lỗi
        |           |         |
        v           |         |
sqlite3_column_*() -+         |      đọc cột của dòng hiện tại (khi SQLITE_ROW)
        |                     |
        v                     |
sqlite3_reset() --------------+      dùng lại statement với tham số mới
        |
        v
sqlite3_finalize()                   giải phóng statement
        |
        v
sqlite3_close()                      đóng database
```

### 3.2. Mở và đóng database

```c
int sqlite3_open_v2(const char *filename, sqlite3 **ppDb, int flags, const char *zVfs);
int sqlite3_close(sqlite3 *db);
int sqlite3_close_v2(sqlite3 *db);
```

Các flag thường dùng:

| Flag | Ý nghĩa |
| --- | --- |
| `SQLITE_OPEN_READONLY` | Chỉ đọc |
| `SQLITE_OPEN_READWRITE` | Đọc ghi, file phải tồn tại |
| `SQLITE_OPEN_CREATE` | Kết hợp với `READWRITE`: tạo file nếu chưa có |
| `SQLITE_OPEN_NOMUTEX` | Connection ở chế độ multi-thread |
| `SQLITE_OPEN_FULLMUTEX` | Connection ở chế độ serialized |

```c
sqlite3 *db = NULL;
int rc = sqlite3_open_v2("/var/lib/app/data.db", &db,
                         SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
if (rc != SQLITE_OK) {
    fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
    sqlite3_close(db);    /* vẫn phải close kể cả khi open lỗi */
    return -1;
}
```

- Tên file đặc biệt `":memory:"` tạo database chỉ nằm trong RAM, mất khi đóng.
- Khi `sqlite3_open_v2()` lỗi, `db` thường vẫn được cấp phát để chứa thông báo lỗi, nên vẫn phải gọi `sqlite3_close()`.
- `sqlite3_close()` trả về `SQLITE_BUSY` và không đóng nếu còn statement chưa `finalize`. `sqlite3_close_v2()` thì đánh dấu đóng và tự giải phóng khi statement cuối cùng được finalize.

### 3.3. Prepared statement

**Ý tưởng**: SQLite không chạy trực tiếp chuỗi SQL mà biên dịch nó thành một chương trình nhỏ (bytecode) rồi mới chạy. Prepared statement tách hai việc này ra:
- **Biên dịch một lần**: `sqlite3_prepare_v2()` tạo ra `sqlite3_stmt`, chứa câu SQL đã biên dịch, các dấu `?` để trống.
- **Chạy nhiều lần**: mỗi lần chỉ điền giá trị mới vào `?` rồi chạy, không phải biên dịch lại.

Giống như viết một hàm có tham số rồi gọi nhiều lần, thay vì viết lại code mỗi lần.

```
prepare   "INSERT INTO reading VALUES (?, ?, ?)"  -> stmt     (1 lần)
   |
   v
bind      ?1 = 1, ?2 = 1761465600, ?3 = 25.4     <----+
   |                                                  |
   v                                                  |
step      thực thi câu lệnh                           |  lặp lại
   |                                                  |
   v                                                  |
reset     đưa stmt về trạng thái ban đầu         -----+
   |
   v
finalize  giải phóng stmt                                 (1 lần)
```

| Bước | Hàm | Làm gì |
| --- | --- | --- |
| 1 | `sqlite3_prepare_v2()` | Kiểm tra cú pháp, kiểm tra bảng/cột có tồn tại, chọn index, sinh bytecode. Sai SQL thì báo lỗi ngay ở bước này. Chưa đụng tới dữ liệu |
| 2 | `sqlite3_bind_*()` | Điền giá trị vào các dấu `?`. Chỉ số bắt đầu từ 1 |
| 3 | `sqlite3_step()` | Chạy câu lệnh. `INSERT`/`UPDATE`/`DELETE` trả về `SQLITE_DONE`. `SELECT` mỗi lần gọi trả về một dòng (`SQLITE_ROW`), hết dòng thì trả về `SQLITE_DONE` |
| 4 | `sqlite3_column_*()` | Đọc cột của dòng hiện tại (chỉ khi `step` trả về `SQLITE_ROW`). Chỉ số bắt đầu từ 0 |
| 5 | `sqlite3_reset()` | Đưa statement về đầu để chạy lại. Giá trị đã bind được giữ nguyên |
| 6 | `sqlite3_finalize()` | Giải phóng statement. Phải gọi trước `sqlite3_close()` |

Vì sao nên dùng:
- **Nhanh hơn** khi chạy lặp lại: bỏ được chi phí biên dịch.
- **An toàn**: giá trị điền vào `?` luôn chỉ là dữ liệu, không bao giờ bị hiểu thành lệnh SQL, nên không bị SQL injection như khi ghép chuỗi bằng `sprintf`.

Ví dụ:

```c
sqlite3_stmt *stmt;

/* 1. Biên dịch một lần */
sqlite3_prepare_v2(db, "INSERT INTO reading (sensor_id, ts, value) VALUES (?, ?, ?);", -1, &stmt, NULL);

double values[] = { 25.4, 25.6, 26.1 };
for (int i = 0; i < 3; i++) {
    /* 2. Điền giá trị */
    sqlite3_bind_int(stmt, 1, 1);
    sqlite3_bind_int64(stmt, 2, 1761465600 + i * 60);
    sqlite3_bind_double(stmt, 3, values[i]);

    /* 3. Chạy */
    if (sqlite3_step(stmt) != SQLITE_DONE)
        fprintf(stderr, "Insert failed: %s\n", sqlite3_errmsg(db));

    /* 5. Về trạng thái ban đầu để dùng lại */
    sqlite3_reset(stmt);
}

/* 6. Giải phóng */
sqlite3_finalize(stmt);
```

Đọc dữ liệu bằng `SELECT`:

```c
sqlite3_prepare_v2(db, "SELECT ts, value FROM reading WHERE sensor_id = ?;", -1, &stmt, NULL);
sqlite3_bind_int(stmt, 1, 1);

/* 3 + 4. Mỗi lần step lấy một dòng, đọc cột theo thứ tự trong SELECT */
while (sqlite3_step(stmt) == SQLITE_ROW) {
    sqlite3_int64 ts = sqlite3_column_int64(stmt, 0);    /* cột 0: ts    */
    double value     = sqlite3_column_double(stmt, 1);   /* cột 1: value */
    printf("ts=%lld value=%.1f\n", (long long)ts, value);
}

sqlite3_finalize(stmt);
```

```
ts=1761465600 value=25.4
ts=1761465660 value=25.6
ts=1761465720 value=26.1
```

#### Lưu ý

- Chuỗi bind bằng biến cục bộ thì dùng `SQLITE_TRANSIENT` để SQLite tự copy: `sqlite3_bind_text(stmt, 1, buf, -1, SQLITE_TRANSIENT)`.
- Con trỏ từ `sqlite3_column_text()` chỉ hợp lệ đến lần `step`/`reset` tiếp theo. Muốn giữ lại thì copy ra.
- `SELECT` đọc dở thì phải `reset` ngay, vì statement đang chạy vẫn giữ lock trên database.

### 3.4. sqlite3_exec và callback

```c
int sqlite3_exec(sqlite3 *db, const char *sql,
                 int (*callback)(void *arg, int ncol, char **values, char **names),
                 void *arg, char **errmsg);
```

`sqlite3_exec()` là hàm tiện ích bọc `prepare` + `step` + `finalize`, chạy được nhiều câu lệnh phân tách bằng `;`. Callback được gọi cho mỗi dòng kết quả, mọi giá trị đều ở dạng chuỗi. Callback trả về khác 0 thì dừng và `sqlite3_exec()` trả về `SQLITE_ABORT`.

```c
static int print_row(void *arg, int ncol, char **values, char **names)
{
    for (int i = 0; i < ncol; i++)
        printf("%s=%s ", names[i], values[i] ? values[i] : "NULL");
    printf("\n");
    return 0;
}

char *err = NULL;
if (sqlite3_exec(db, "SELECT id, name FROM sensor;", print_row, NULL, &err) != SQLITE_OK) {
    fprintf(stderr, "SQL error: %s\n", err);
    sqlite3_free(err);    /* errmsg do SQLite cấp phát, phải giải phóng bằng sqlite3_free */
}
```

Nên dùng `sqlite3_exec()` cho câu lệnh không có tham số: tạo bảng, `PRAGMA`, `BEGIN`/`COMMIT`. Với truy vấn có tham số hoặc chạy lặp lại, dùng prepared statement.

### 3.5. Xử lý mã lỗi

```c
int sqlite3_errcode(sqlite3 *db);           /* mã lỗi của API gần nhất */
int sqlite3_extended_errcode(sqlite3 *db);  /* mã lỗi mở rộng, chi tiết hơn */
const char *sqlite3_errmsg(sqlite3 *db);    /* thông báo lỗi của API gần nhất */
const char *sqlite3_errstr(int rc);         /* mã lỗi -> chuỗi mô tả */
int sqlite3_extended_result_codes(sqlite3 *db, int onoff);  /* API trả về mã mở rộng */
```

| Mã lỗi | Giá trị | Ý nghĩa | Cách xử lý |
| --- | --- | --- | --- |
| `SQLITE_OK` | 0 | Thành công | |
| `SQLITE_ERROR` | 1 | Lỗi chung: sai cú pháp SQL, bảng không tồn tại,... | Lỗi lập trình, sửa code |
| `SQLITE_BUSY` | 5 | Database đang bị lock bởi connection khác | Đặt `busy_timeout`, retry (mục 4.3) |
| `SQLITE_LOCKED` | 6 | Xung đột lock trong cùng một connection (hoặc shared cache) | Thường do chưa reset/finalize statement đang đọc |
| `SQLITE_NOMEM` | 7 | Hết bộ nhớ | Giảm cache, kiểm tra rò rỉ |
| `SQLITE_READONLY` | 8 | Ghi vào database chỉ đọc | Kiểm tra quyền file và thư mục chứa file |
| `SQLITE_IOERR` | 10 | Lỗi I/O khi đọc/ghi | Kiểm tra storage, filesystem, log kernel (`dmesg`) |
| `SQLITE_CORRUPT` | 11 | File database bị hỏng | Chạy `integrity_check`, phục hồi (mục 5.3) |
| `SQLITE_FULL` | 13 | Hết dung lượng disk hoặc vượt `max_page_count` | Xóa dữ liệu cũ (mục 8.2) |
| `SQLITE_CANTOPEN` | 14 | Không mở được file | Kiểm tra đường dẫn, quyền, thư mục tồn tại |
| `SQLITE_CONSTRAINT` | 19 | Vi phạm ràng buộc: UNIQUE, NOT NULL, CHECK, FOREIGN KEY | Dữ liệu sai hoặc trùng |
| `SQLITE_MISUSE` | 21 | Dùng API sai cách (dùng statement đã finalize,...) | Lỗi lập trình |
| `SQLITE_NOTADB` | 26 | File không phải database SQLite | Sai file, hoặc file bị ghi đè hoàn toàn |
| `SQLITE_ROW` | 100 | `step` trả về một dòng | |
| `SQLITE_DONE` | 101 | `step` thực thi xong | |

Mã mở rộng cho biết chi tiết hơn, ví dụ `SQLITE_CONSTRAINT_UNIQUE`, `SQLITE_CONSTRAINT_NOTNULL`, `SQLITE_IOERR_FSYNC`, `SQLITE_BUSY_SNAPSHOT`. Mã cơ bản là 8 bit thấp: `(ext_rc & 0xFF)`.

### 3.6. Ví dụ hoàn chỉnh

Chương trình mô phỏng một collector: tạo schema, ghi dữ liệu cảm biến theo lô trong transaction, rồi đọc lại.

```c
/* sensor_db.c */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sqlite3.h>

#define DB_PATH "sensor.db"

static const char *SCHEMA_SQL =
    "PRAGMA journal_mode = WAL;"
    "PRAGMA synchronous = NORMAL;"
    "PRAGMA foreign_keys = ON;"
    "CREATE TABLE IF NOT EXISTS reading ("
    "    id        INTEGER PRIMARY KEY,"
    "    sensor_id INTEGER NOT NULL,"
    "    ts        INTEGER NOT NULL,"
    "    value     REAL    NOT NULL"
    ");"
    "CREATE INDEX IF NOT EXISTS idx_reading_sensor_ts ON reading(sensor_id, ts);";

/* Chạy câu lệnh không có tham số, in lỗi nếu có */
static int exec_sql(sqlite3 *db, const char *sql)
{
    char *err = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error (%d): %s\n", rc, err);
        sqlite3_free(err);
    }
    return rc;
}

/* Ghi n mẫu trong một transaction, tái sử dụng một prepared statement */
static int insert_batch(sqlite3 *db, sqlite3_stmt *stmt, int sensor_id, int n)
{
    int rc = exec_sql(db, "BEGIN IMMEDIATE;");
    if (rc != SQLITE_OK)
        return rc;

    time_t now = time(NULL);
    for (int i = 0; i < n; i++) {
        sqlite3_bind_int(stmt, 1, sensor_id);
        sqlite3_bind_int64(stmt, 2, (sqlite3_int64)now + i);
        sqlite3_bind_double(stmt, 3, 20.0 + (rand() % 100) / 10.0);

        rc = sqlite3_step(stmt);
        sqlite3_reset(stmt);
        if (rc != SQLITE_DONE) {
            fprintf(stderr, "Insert failed: %s\n", sqlite3_errmsg(db));
            exec_sql(db, "ROLLBACK;");
            return rc;
        }
    }

    return exec_sql(db, "COMMIT;");
}

/* Đọc các mẫu gần nhất của một sensor */
static void print_latest(sqlite3 *db, int sensor_id, int limit)
{
    sqlite3_stmt *stmt;
    const char *sql = "SELECT ts, value FROM reading "
                      "WHERE sensor_id = ? ORDER BY ts DESC LIMIT ?;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "Prepare failed: %s\n", sqlite3_errmsg(db));
        return;
    }

    sqlite3_bind_int(stmt, 1, sensor_id);
    sqlite3_bind_int(stmt, 2, limit);

    int rc;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        sqlite3_int64 ts = sqlite3_column_int64(stmt, 0);
        double value = sqlite3_column_double(stmt, 1);
        printf("sensor=%d ts=%lld value=%.1f\n", sensor_id, (long long)ts, value);
    }
    if (rc != SQLITE_DONE)
        fprintf(stderr, "Query failed: %s\n", sqlite3_errmsg(db));

    sqlite3_finalize(stmt);
}

int main(void)
{
    sqlite3 *db = NULL;
    sqlite3_stmt *ins = NULL;
    int ret = EXIT_FAILURE;

    if (sqlite3_open_v2(DB_PATH, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL) != SQLITE_OK) {
        fprintf(stderr, "Cannot open %s: %s\n", DB_PATH, sqlite3_errmsg(db));
        goto out;
    }

    /* Chờ tối đa 5s nếu database đang bị tiến trình khác lock */
    sqlite3_busy_timeout(db, 5000);

    if (exec_sql(db, SCHEMA_SQL) != SQLITE_OK)
        goto out;

    if (sqlite3_prepare_v2(db, "INSERT INTO reading (sensor_id, ts, value) VALUES (?, ?, ?);",
                           -1, &ins, NULL) != SQLITE_OK) {
        fprintf(stderr, "Prepare failed: %s\n", sqlite3_errmsg(db));
        goto out;
    }

    if (insert_batch(db, ins, 1, 100) != SQLITE_OK)
        goto out;
    printf("Inserted 100 rows, total changes: %d\n", sqlite3_total_changes(db));

    print_latest(db, 1, 5);
    ret = EXIT_SUCCESS;

out:
    sqlite3_finalize(ins);    /* finalize(NULL) là hợp lệ */
    sqlite3_close(db);
    return ret;
}
```

Build và chạy:

```bash
gcc sensor_db.c -o sensor_db -lsqlite3
./sensor_db
```

```
Inserted 100 rows, total changes: 100
sensor=1 ts=1761466099 value=27.3
sensor=1 ts=1761466098 value=21.8
sensor=1 ts=1761466097 value=29.0
sensor=1 ts=1761466096 value=24.5
sensor=1 ts=1761466095 value=20.6
```

### 3.7. C++ wrapper: SQLiteCpp

[SQLiteCpp](https://github.com/SRombauts/SQLiteCpp) là wrapper C++ nhỏ gọn, dùng RAII nên không phải tự gọi `finalize`/`close` và báo lỗi bằng exception `SQLite::Exception`.

```cmake
# Thêm vào project dạng submodule
add_subdirectory(third_party/SQLiteCpp)
target_link_libraries(app PRIVATE SQLiteCpp)
```

```cpp
#include <SQLiteCpp/SQLiteCpp.h>
#include <iostream>

int main()
{
    try {
        SQLite::Database db("sensor.db", SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
        db.setBusyTimeout(5000);
        db.exec("CREATE TABLE IF NOT EXISTS reading ("
                "id INTEGER PRIMARY KEY, sensor_id INTEGER, ts INTEGER, value REAL)");

        // Transaction tự ROLLBACK trong destructor nếu chưa commit (ví dụ khi có exception)
        SQLite::Transaction tx(db);
        SQLite::Statement ins(db, "INSERT INTO reading (sensor_id, ts, value) VALUES (?, ?, ?)");
        for (int i = 0; i < 100; i++) {
            ins.bind(1, 1);
            ins.bind(2, static_cast<int64_t>(1761465600 + i));
            ins.bind(3, 25.0 + i * 0.1);
            ins.exec();
            ins.reset();
        }
        tx.commit();

        SQLite::Statement query(db, "SELECT ts, value FROM reading WHERE sensor_id = ? LIMIT 3");
        query.bind(1, 1);
        while (query.executeStep()) {
            int64_t ts = query.getColumn(0);
            double value = query.getColumn(1);
            std::cout << "ts=" << ts << " value=" << value << "\n";
        }
    } catch (const SQLite::Exception &e) {
        std::cerr << "SQLite error: " << e.what() << " (code " << e.getErrorCode() << ")\n";
        return 1;
    }
    return 0;
}
```

SQLiteCpp có thể dùng sqlite3 của hệ thống (`SQLITECPP_INTERNAL_SQLITE=OFF`) hoặc tự build bản amalgamation đi kèm.

### 3.8. Python sqlite3

Module `sqlite3` có sẵn trong thư viện chuẩn Python, tiện để viết prototype, script phân tích dữ liệu hoặc ứng dụng gateway chạy Python.

```python
import sqlite3
import time

conn = sqlite3.connect("sensor.db", timeout=5.0)   # timeout = busy_timeout (giây)
conn.execute("PRAGMA journal_mode = WAL")
conn.execute("""
    CREATE TABLE IF NOT EXISTS reading (
        id INTEGER PRIMARY KEY, sensor_id INTEGER, ts INTEGER, value REAL
    )
""")

# "with conn" tự COMMIT khi thành công, tự ROLLBACK khi có exception
now = int(time.time())
rows = [(1, now + i, 25.0 + i * 0.1) for i in range(1000)]
with conn:
    conn.executemany("INSERT INTO reading (sensor_id, ts, value) VALUES (?, ?, ?)", rows)

conn.row_factory = sqlite3.Row      # truy cập cột theo tên
for row in conn.execute("SELECT ts, value FROM reading ORDER BY ts DESC LIMIT 3"):
    print(f"ts={row['ts']} value={row['value']:.1f}")

conn.close()
```

:::warning Lưu ý
Module `sqlite3` của Python (trước 3.12, hoặc khi không đặt `autocommit`) tự mở transaction ngầm trước các câu `INSERT`/`UPDATE`/`DELETE` và chỉ ghi xuống khi gọi `commit()`. Quên `commit()` thì dữ liệu mất khi đóng chương trình. Dùng `with conn:` để không phải nhớ.
:::

## 4. Các cơ chế cốt lõi

### 4.1. Transaction và ACID

**Transaction** là một nhóm câu lệnh được xử lý như một khối duy nhất: hoặc tất cả có hiệu lực hoặc không câu nào có hiệu lực (cú pháp ở mục 1.2.9).

- Không viết `BEGIN` thì SQLite ở chế độ **autocommit**: mỗi câu lệnh tự động là một transaction riêng, tự commit khi chạy xong.
- Viết `BEGIN` thì autocommit tạm tắt cho đến khi gặp `COMMIT` hoặc `ROLLBACK`.

Ví dụ trên thiết bị nhúng: bạn cập nhật cấu hình mạng gồm 3 giá trị là IP, gateway và DNS.

```sql
UPDATE config SET value = '192.168.1.50' WHERE key = 'ip';
UPDATE config SET value = '192.168.1.1'  WHERE key = 'gateway';
UPDATE config SET value = '8.8.8.8'      WHERE key = 'dns';
```

Nếu mất điện sau câu lệnh thứ nhất, thiết bị khởi động lại với IP mới nhưng gateway cũ, có thể mất kết nối mạng hoàn toàn. Đặt chúng trong transaction sẽ loại bỏ tình huống này:

```sql
BEGIN;
UPDATE config SET value = '192.168.1.50' WHERE key = 'ip';
UPDATE config SET value = '192.168.1.1'  WHERE key = 'gateway';
UPDATE config SET value = '8.8.8.8'      WHERE key = 'dns';
COMMIT;
```

Mất điện ở bất kỳ thời điểm nào trước khi `COMMIT` hoàn tất, cả ba giá trị đều giữ nguyên bản cũ. Sau khi `COMMIT` hoàn tất, cả ba đều là bản mới. Không bao giờ có trạng thái nửa vời.

SQLite bảo đảm 4 tính chất ACID cho mỗi transaction:

| Tính chất | Ý nghĩa | SQLite làm bằng cách nào |
| --- | --- | --- |
| **A**tomicity | Không có trạng thái nửa cũ nửa mới | Lưu thông tin khôi phục vào journal/WAL trước khi sửa database (mục 4.2) |
| **C**onsistency | Dữ liệu luôn thỏa mãn các ràng buộc | Kiểm tra `NOT NULL`, `UNIQUE`, `CHECK`, khóa ngoại. Vi phạm thì câu lệnh bị hủy |
| **I**solation | Transaction này không thấy thay đổi dở dang của transaction khác | Lock trên toàn database (mục 4.3), reader chỉ thấy dữ liệu đã commit |
| **D**urability | Đã commit thì không mất, kể cả khi mất điện | Gọi `fsync()` lúc commit, mức độ tùy `PRAGMA synchronous` (mục 4.5) |

Một transaction ghi đi qua các bước:

```
BEGIN
  |
  v
Lấy lock ghi (mục 4.3)
  |
  v
Sửa các page trong bộ nhớ (page cache)
  |
  +--> ROLLBACK: bỏ các page đã sửa, database giữ nguyên như trước BEGIN
  |
  v
COMMIT:  ghi thay đổi vào journal hoặc WAL -> fsync
  |
  v
Nhả lock, các connection khác thấy dữ liệu mới
```

`ROLLBACK` hủy mọi thay đổi kể từ `BEGIN`, database quay về đúng trạng thái trước transaction:

```sql
BEGIN;
DELETE FROM reading WHERE sensor_id = 2;
SELECT COUNT(*) FROM reading WHERE sensor_id = 2;    -- 0: trong transaction đã thấy bị xóa
ROLLBACK;
SELECT COUNT(*) FROM reading WHERE sensor_id = 2;    -- 2: dữ liệu vẫn còn nguyên
```

Ngoài việc gọi `ROLLBACK` chủ động, rollback còn xảy ra tự động trong các trường hợp:

| Tình huống | Điều gì xảy ra |
| --- | --- |
| Chương trình crash hoặc mất điện trước `COMMIT` | Lần mở database tiếp theo, SQLite dùng journal/WAL để bỏ phần chưa commit (mục 4.2) |
| Đóng connection khi transaction còn mở | Transaction bị rollback |
| Lỗi nặng: `SQLITE_FULL`, `SQLITE_IOERR`, `SQLITE_NOMEM` | SQLite có thể tự rollback cả transaction |

**Lỗi ở một câu lệnh không tự rollback cả transaction.** Khi một câu lệnh vi phạm ràng buộc, SQLite chỉ hủy riêng câu lệnh đó, transaction vẫn mở và các câu lệnh trước vẫn còn:

```sql
-- bảng sensor đã có 'temp0'
BEGIN;
INSERT INTO sensor (name, unit) VALUES ('temp1', 'C');    -- thành công
INSERT INTO sensor (name, unit) VALUES ('temp0', 'C');    -- lỗi UNIQUE, chỉ câu này bị hủy
COMMIT;                                                   -- temp1 vẫn được ghi
```

Nếu muốn lỗi bất kỳ câu nào thì hủy hết, ứng dụng phải tự kiểm tra lỗi và gọi `ROLLBACK`. Muốn biết transaction còn mở hay đã bị SQLite tự rollback, dùng `sqlite3_get_autocommit(db)`: trả về khác 0 nghĩa là không còn transaction nào đang mở.

Muốn chỉ hủy một phần transaction thì dùng `SAVEPOINT` / `ROLLBACK TO` (mục 1.2.9).

### 4.2. Journal: Rollback journal và WAL

**Vấn đề cần giải quyết**: một transaction thường sửa nhiều page (page bảng, page index,...). Nếu mất điện khi mới ghi được một nửa số page xuống file, database rơi vào trạng thái nửa cũ nửa mới. Journal là file phụ giúp SQLite luôn khôi phục được về trạng thái nhất quán. SQLite có hai cơ chế journal.

**Rollback journal** (mặc định): lưu bản cũ ra ngoài, sửa trực tiếp file database.

Các file liên quan

```
data.db           <- database chính
data.db-journal   <- chứa bản sao các page gốc trước khi bị sửa
```

Quy trình một transaction ghi:

```
1. Copy nội dung gốc của các page sắp sửa vào data.db-journal
2. fsync                <- đảm bảo bản copy đã thật sự nằm trên flash
3. Ghi nội dung mới vào data.db
4. fsync                <- đảm bảo dữ liệu mới đã nằm trên flash
5. Xóa data.db-journal  <- đây chính là thời điểm COMMIT
```

Mất điện trước bước 5: Khi mở database, nếu SQLite thấy file journal vẫn còn tồn tại (hot journal), nó hiểu rằng transaction trước đó chưa hoàn tất. SQLite sẽ chép các page gốc từ journal ngược vào database, tức là rollback về trạng thái trước transaction.

Các biến thể của rollback journal (`PRAGMA journal_mode`):

| Mode | Ghi chú |
| --- | --- |
| `DELETE` (mặc định) | Xóa file journal khi commit |
| `TRUNCATE`, `PERSIST` | Cắt file về 0 byte hoặc chỉ xóa header thay vì xóa file. Nhanh hơn `DELETE` trên nhiều filesystem |
| `MEMORY` | Journal nằm trong RAM. **Mất điện giữa transaction có thể hỏng database** |
| `OFF` | Không có journal, không rollback được, rất nguy hiểm |

Nhược điểm chính
- Writer chặn reader: trong lúc commit (giữ lock EXCLUSIVE), không tiến trình nào đọc được.
- Nhiều fsync: ít nhất 2 lần cho mỗi transaction (journal và database), cộng thêm sync thư mục ở chế độ `DELETE`.
- Ghi ngẫu nhiên vào database ở nhiều vị trí khác nhau.

**WAL - Write-Ahead Log**: WAL đảo ngược cách làm. Giữ nguyên file database, ghi bản mới nối vào cuối file `-wal`.

Các file liên quan

```
data.db        <- database chính (không bị sửa khi commit)
data.db-wal    <- log, các page mới được nối (append) vào cuối
data.db-shm    <- wal-index, chỉ mục nằm trong shared memory (mmap)
```

Quy trình ghi:

```
1. Ghi nội dung mới của các page vào cuối data.db-wal, kèm commit record, fsync   <- COMMIT
2. Reader đọc page: có trong -wal thì lấy bản trong -wal, không có thì đọc data.db
   (file data.db-shm là bảng tra cứu page nào đang nằm trong -wal)
3. Checkpoint (định kỳ): chép các page từ -wal về data.db, rồi ghi lại -wal từ đầu
```

Quy trình đọc:

```
Reader cần page số N:
  ├─ Tra wal-index (-shm): page N có bản mới trong WAL không?
  │     ├─ Có    -> đọc từ data.db-wal
  │     └─ Không -> đọc từ data.db
```

Mỗi reader khi bắt đầu sẽ ghi nhớ vị trí commit frame cuối cùng lúc đó gọi là snapshot. Reader chỉ nhìn thấy dữ liệu đến điểm đó nên writer có thể tiếp tục append mà không ảnh hưởng reader. Đây là lý do reader và writer chạy song song được.

Mất điện: Khi mở lại database, SQLite đọc WAL và chỉ chấp nhận các frame thuộc transaction đã có commit frame hợp lệ. Các frame của transaction ghi dở bị bỏ qua. Không cần rollback vì database chính chưa bao giờ bị sửa dở.

Checkpoint: WAL không thể phình mãi nên định kỳ SQLite chép các page mới nhất từ WAL về `data.db`. Thao tác này gọi là checkpoint.
- Tự động: mặc định khi WAL đạt khoảng 1000 page (`PRAGMA wal_autocheckpoint`).
- Thủ công: `PRAGMA wal_checkpoint(PASSIVE | FULL | RESTART | TRUNCATE);`
- Giới hạn: checkpoint không thể chép đè lên những page mà một reader cũ vẫn đang cần. Nếu luôn có reader chạy lâu, WAL sẽ phình to liên tục (hiện tượng *checkpoint starvation*).

**So sánh**

| | Rollback journal | WAL |
| --- | --- | --- |
| Ghi | Ghi 2 lần (journal + database), nhiều `fsync` | Ghi nối tiếp vào `-wal`, ít `fsync` hơn |
| Đọc và ghi đồng thời | Không: đang commit thì không ai đọc được | Có: reader và writer không chặn nhau |
| File phụ | `-journal` (chỉ tồn tại trong lúc ghi) | `-wal`, `-shm` (tồn tại khi database đang mở) |
| Hạn chế | | Mọi tiến trình phải cùng một máy. Cần checkpoint, `-wal` có thể phình to |

:::warning Lưu ý
Ở chế độ WAL, dữ liệu mới nhất có thể vẫn nằm trong `-wal`. Không xóa file `-wal`/`-shm` và không copy riêng file `.db` khi database đang mở (dùng `.backup` hoặc `VACUUM INTO`, mục 1.4).
:::

### 4.3. Locking và truy cập đồng thời

SQLite không có server điều phối. Mọi tiến trình và luồng truy cập trực tiếp vào cùng một file nên chúng phối hợp với nhau bằng lock của hệ điều hành. Quy tắc chung:
- Nhiều reader có thể đọc cùng lúc.
- Chỉ một writer tại một thời điểm ở cấp toàn database. SQLite không có lock theo bảng hay theo dòng như MySQL/PostgreSQL.

Cách các reader và writer tương tác phụ thuộc vào journal mode: rollback journal và WAL dùng hai cơ chế lock khác nhau.

**Ở chế độ rollback journal**, mỗi connection đi qua các mức lock:

```
UNLOCKED --> SHARED --> RESERVED --> PENDING --> EXCLUSIVE
             (đọc)      (định ghi)   (chờ reader  (đang ghi
                                      rời đi)      xuống file)
```

| Trạng thái    | Ý nghĩa   | Số connection giữ được |
| --- | --- | --- |
| `UNLOCKED`    | Không truy cập | Không giới hạn |
| `SHARED`      | Đang đọc | Nhiều |
| `RESERVED`    | Định ghi, đang chuẩn bị thay đổi trong cache. Reader khác vẫn đọc được | 1 |
| `PENDING`     | Đang chờ các reader hiện tại thoát. Không cho reader mới vào | 1 |
| `EXCLUSIVE`   | Đang ghi vào file database | 1 và không còn ai khác |

Hệ quả: writer phải chờ mọi reader đọc xong mới commit được và trong lúc commit thì không ai đọc được.

**Ở chế độ WAL**: WAL dùng lock trong file `-shm` (shared memory) thay vì lock trên file database:

| Lock | Vai trò |
| --- | --- |
| `WAL_WRITE_LOCK` | Chỉ một writer được append vào WAL |
| `WAL_CKPT_LOCK` | Chỉ một checkpoint chạy tại một thời điểm |
| `WAL_RECOVER_LOCK` | Dùng khi dựng lại wal-index sau crash |
| `WAL_READ_LOCK(n)` | Các read mark slot. Mỗi reader giữ một slot ghi lại snapshot của nó |

**SQLITE_BUSY**

Khi không lấy được lock, SQLite không tự chờ. Mặc định nó trả về `SQLITE_BUSY` (“database is locked”) ngay lập tức. Đặt `busy_timeout` để SQLite tự chờ và thử lại:

```c
sqlite3_busy_timeout(db, 5000);       /* hoặc: PRAGMA busy_timeout = 5000; */
```

Có một trường hợp `busy_timeout` không giúp được:

```
Connection A                         Connection B
BEGIN; SELECT ...   (SHARED)         BEGIN; SELECT ...   (SHARED)
INSERT ...          (RESERVED)
                                     INSERT ... -> SQLITE_BUSY ngay
COMMIT -> chờ B nhả SHARED
```

A chờ B nhả `SHARED`, B lại muốn ghi nên phải chờ A: hai bên deadlock. SQLite phát hiện tình huống này và trả `SQLITE_BUSY` ngay cho B mà không chờ timeout. Cách tránh: transaction có ghi thì bắt đầu bằng **`BEGIN IMMEDIATE`** để lấy quyền ghi ngay từ đầu.

| Câu lệnh | Lấy lock ghi khi nào | Dùng cho |
| --- | --- | --- |
| `BEGIN` (`DEFERRED`) | Ở câu lệnh ghi đầu tiên | Transaction chỉ đọc |
| `BEGIN IMMEDIATE` | Ngay lập tức (chờ theo `busy_timeout`) | Transaction có ghi |
| `BEGIN EXCLUSIVE` | Ngay lập tức, chặn cả reader (rollback journal) | Hiếm khi cần |

Tóm lại, khi có nhiều tiến trình hoặc thread cùng truy cập:
- Dùng `journal_mode = WAL`.
- Đặt `busy_timeout` cho mọi connection.
- Transaction có ghi dùng `BEGIN IMMEDIATE`, giữ transaction ngắn.
- `reset` statement `SELECT` ngay khi đọc xong, vì statement đang chạy dở vẫn giữ lock.

### 4.4. Index và query planner

**Cách SQLite lưu dữ liệu**: mỗi bảng là một B-tree sắp xếp theo `rowid`. Mỗi index là **một B-tree riêng**, sắp xếp theo các cột được index, mỗi phần tử trỏ về `rowid` của dòng tương ứng.

```
Index idx_reading_sensor_ts              Bảng reading
+-----------+-----+-------+              +-------+-----------+-----+-------+
| sensor_id | ts  | rowid |              | rowid | sensor_id | ts  | value |
+-----------+-----+-------+              +-------+-----------+-----+-------+
| 1         | 600 | 1     |  ---> dòng 1 | 1     | 1         | 600 | 25.4  |
| 1         | 660 | 3     |  ---> dòng 3 | 2     | 2         | 600 | 61.0  |
| 2         | 600 | 2     |  ---> dòng 2 | 3     | 1         | 660 | 25.6  |
+-----------+-----+-------+              +-------+-----------+-----+-------+
                                         (ts viết tắt 3 chữ số cuối)
```

Truy vấn `WHERE sensor_id = 1 AND ts > ...`:
- Không có index: đọc lần lượt mọi dòng của bảng (`SCAN`).
- Có index: tìm nhị phân trong index tới đúng đoạn `sensor_id = 1`, lấy các `rowid`, rồi đọc đúng các dòng đó (`SEARCH`).
- Nếu index chứa đủ mọi cột mà truy vấn cần (**covering index**), SQLite không cần đọc bảng nữa.

Cái giá phải trả: mỗi `INSERT`/`UPDATE`/`DELETE` phải cập nhật thêm B-tree của mọi index nên ghi chậm hơn và ghi xuống flash nhiều hơn.

**Query planner** là thành phần quyết định cách chạy mỗi truy vấn: dùng index nào, duyệt bảng nào trước khi JOIN. Xem quyết định của nó bằng `EXPLAIN QUERY PLAN`:

```sql
EXPLAIN QUERY PLAN
SELECT ts, value FROM reading WHERE sensor_id = 1 AND ts > 1761400000;
```

```
-- Chưa có index
`--SCAN reading

-- Sau khi CREATE INDEX idx_reading_sensor_ts ON reading(sensor_id, ts);
`--SEARCH reading USING INDEX idx_reading_sensor_ts (sensor_id=? AND ts>?)
```

| Kết quả | Ý nghĩa |
| --- | --- |
| `SCAN bảng` | Duyệt toàn bộ bảng: chậm với bảng lớn |
| `SEARCH bảng USING INDEX ...` | Dùng index tìm đúng dòng |
| `USING COVERING INDEX` | Chỉ đọc index, không đọc bảng: nhanh nhất |
| `USE TEMP B-TREE FOR ORDER BY` | Phải sắp xếp tạm: có thể thêm cột `ORDER BY` vào index |

Nguyên tắc tạo index:
- Index nhiều cột: cột so sánh `=` đặt trước, cột so sánh khoảng (`>`, `<`, `BETWEEN`) hoặc `ORDER BY` đặt sau. Index `(sensor_id, ts)` dùng được cho `WHERE sensor_id = ?`, nhưng **không** dùng được cho `WHERE ts > ?` đơn lẻ.
- Không bọc cột trong hàm: `WHERE date(ts, 'unixepoch') = '2025-10-26'` không dùng được index, viết thành `WHERE ts >= ? AND ts < ?`.
- Chỉ tạo index cho truy vấn thật sự dùng, kiểm tra lại bằng `EXPLAIN QUERY PLAN`.

Query planner chọn tốt hơn khi có thống kê về dữ liệu:

```sql
PRAGMA optimize;     -- nhẹ, nên chạy định kỳ hoặc trước khi đóng connection
ANALYZE;             -- thu thập thống kê đầy đủ
```

### 4.5. PRAGMA

PRAGMA là câu lệnh đặc biệt của SQLite để xem hoặc thay đổi cấu hình của database và connection.

```sql
PRAGMA journal_mode;                  -- đọc giá trị
PRAGMA journal_mode = WAL;            -- đặt giá trị
SELECT * FROM pragma_table_info('reading');   -- dùng PRAGMA như một bảng trong SELECT
```

Điều quan trọng nhất cần nhớ là phạm vi của mỗi PRAGMA:
- **Theo connection**: mất khi đóng connection, phải đặt lại mỗi lần mở database.
- **Lưu trong file database**: đặt một lần là giữ mãi, mọi connection đều thấy.

PRAGMA thường dùng:

| PRAGMA | Phạm vi | Tác dụng |
| --- | --- | --- |
| `journal_mode` | File (với `WAL`) | Cơ chế journal (mục 4.2) |
| `synchronous` | Connection | Mức độ `fsync` (bảng dưới) |
| `busy_timeout` | Connection | Thời gian chờ lock (mục 4.3) |
| `foreign_keys` | Connection | Bật kiểm tra khóa ngoại, mặc định tắt |
| `cache_size` | Connection | Kích thước page cache. Số âm là KiB, ví dụ `-2000` = 2 MB (mặc định) |
| `temp_store` | Connection | `MEMORY`: bảng tạm khi sort, group by nằm trong RAM, không ghi flash |
| `mmap_size` | Connection | Đọc file qua mmap, bớt copy dữ liệu. Lỗi I/O khi dùng mmap gây `SIGBUS`, nên để `0` nếu storage kém tin cậy |
| `page_size` | File | Kích thước page (mặc định 4096). Chỉ đổi được trước khi tạo bảng, hoặc kèm `VACUUM` |
| `auto_vacuum` | File | Tự trả dung lượng trống cho filesystem (mục 8.2) |
| `user_version` | File | Số nguyên tự do, dùng đánh dấu phiên bản schema (mục 8.6) |
| `integrity_check`, `quick_check` | | Kiểm tra database có bị hỏng không (mục 5.3) |

**PRAGMA synchronous** quyết định SQLite gọi `fsync()` nhiều hay ít, tức là đánh đổi giữa an toàn dữ liệu và tốc độ:

| Giá trị | Rollback journal | WAL |
| --- | --- | --- |
| `OFF` | Không `fsync`. Mất điện có thể **hỏng database** | Như bên trái |
| `NORMAL` | Ít `fsync` hơn, rất hiếm khi hỏng | **Không hỏng database**, nhưng có thể mất vài transaction cuối khi mất điện. Chỉ `fsync` lúc checkpoint |
| `FULL` (mặc định) | An toàn | An toàn, không mất transaction đã commit |
| `EXTRA` | An toàn hơn `FULL` một chút | Như `FULL` |

Vì các PRAGMA theo connection mất khi đóng, nên gom chúng vào một hàm gọi ngay sau `sqlite3_open_v2()`:

```c
static const char *INIT_SQL =
    "PRAGMA journal_mode = WAL;"
    "PRAGMA synchronous = NORMAL;"
    "PRAGMA busy_timeout = 5000;"
    "PRAGMA foreign_keys = ON;"
    "PRAGMA temp_store = MEMORY;"
    "PRAGMA cache_size = -1024;";       /* 1 MB, giảm nếu thiết bị ít RAM */

sqlite3_exec(db, INIT_SQL, NULL, NULL, NULL);
```

## 5. Lưu trữ trên Flash và độ tin cậy

Trên PC, ổ cứng bền và nguồn điện ổn định. Trên thiết bị embedded, flash có giới hạn số lần ghi và thiết bị có thể bị rút điện bất cứ lúc nào. Phần này áp dụng các cơ chế ở mục 4 vào điều kiện đó.

### 5.1. Các loại bộ nhớ Flash

| Loại | Có FTL/controller | Số lần ghi/xóa mỗi block | Filesystem thường dùng | Ghi chú |
| --- | --- | --- | --- | --- |
| NOR | Không | ~100.000 | JFFS2, UBIFS, littlefs | Dung lượng nhỏ, đọc nhanh, ghi/xóa chậm |
| Raw NAND (SLC) | Không | ~50.000 - 100.000 | UBIFS | Có bad block, cần ECC, phần mềm phải tự làm wear leveling |
| eMMC | Có | Tùy chip (MLC/TLC, có chế độ pSLC) | ext4, F2FS | Controller bên trong lo wear leveling, bad block, ECC |
| SD card | Có | Rất khác nhau giữa các hãng | ext4, F2FS | Thẻ rẻ thường có FTL kém, dễ hỏng khi mất điện |

- **Flash có FTL** (eMMC, SD): Linux thấy như block device bình thường (`/dev/mmcblk0`), wear leveling do controller lo.
- **Raw flash** (NAND, NOR): Linux thấy qua MTD (`/dev/mtd0`), UBI lo wear leveling và bad block.

Flash không ghi đè trực tiếp được: muốn ghi lại phải xóa cả block (128 KB - vài MB) trong khi đơn vị ghi chỉ là page (2 - 16 KB). Đây là nguồn gốc của write amplification (mục 5.5).

Kiểm tra tuổi thọ eMMC:

```bash
mmc extcsd read /dev/mmcblk0 | grep -i -E "LIFE_TIME|PRE_EOL"
# eMMC Life Time Estimation A [EXT_CSD_DEVICE_LIFE_TIME_EST_TYP_A]: 0x01
# eMMC Pre EOL information [EXT_CSD_PRE_EOL_INFO]: 0x01
```

`LIFE_TIME_EST` từ `0x01` (đã dùng 0-10% tuổi thọ) đến `0x0B` (vượt tuổi thọ thiết kế). `PRE_EOL_INFO`: `0x01` bình thường, `0x02` cảnh báo, `0x03` khẩn cấp.

### 5.2. Filesystem

| Filesystem | Dùng trên | Với SQLite |
| --- | --- | --- |
| ext4 | eMMC, SD | Hoạt động tốt. Giữ mặc định `barrier=1`, không dùng `nobarrier` |
| F2FS | eMMC, SD | Thiết kế cho flash, thường ít write amplification hơn ext4 |
| UBIFS | Raw NAND | Chịu mất điện tốt, hiệu năng `fsync` cần đo thực tế |
| JFFS2 | NOR nhỏ | Không hỗ trợ shared mmap, WAL chỉ chạy được với `PRAGMA locking_mode=EXCLUSIVE` |
| tmpfs | RAM | Mất khi khởi động lại, dùng cho database tạm |

```bash
# /etc/fstab
/dev/mmcblk0p3  /data  ext4  defaults,noatime  0  2
```

- `noatime`: không ghi metadata mỗi lần đọc file.
- Đặt database trên phân vùng dữ liệu riêng (`/data`), tách khỏi rootfs. Kết hợp rootfs read-only giúp thiết bị luôn khởi động được.

### 5.3. Mất điện và hỏng database

Rollback journal và WAL (mục 4.2) giữ database nhất quán khi mất điện, với một giả định: **khi `fsync()` trả về thì dữ liệu đã thực sự nằm trên flash**. Database chỉ hỏng khi giả định này sai hoặc cơ chế bảo vệ bị tắt:

| Nguyên nhân | Giải thích |
| --- | --- |
| Storage nói dối về `fsync` | Thẻ SD/eMMC rẻ báo đã ghi xong khi dữ liệu còn trong cache của controller |
| `synchronous = OFF`, `journal_mode = OFF/MEMORY` | Tắt cơ chế bảo vệ |
| Xóa file `-journal`/`-wal` thủ công, copy file `.db` khi đang mở | Mất thông tin để khôi phục |
| Database trên NFS/SMB | Lock không hoạt động đúng |
| Giữ connection qua `fork()` | Tiến trình cha và con dùng chung trạng thái connection |
| Flash hết tuổi thọ | Bit lỗi, block hỏng |

Kiểm tra và phục hồi:

```sql
PRAGMA quick_check;        -- nhanh; kết quả ok là database không lỗi
PRAGMA integrity_check;    -- đầy đủ, chậm hơn
```

```bash
sqlite3 broken.db ".recover" | sqlite3 recovered.db     # cứu được nhiều dữ liệu nhất có thể
```

Chiến lược trên sản phẩm:
- Chạy `quick_check` khi ứng dụng khởi động.
- Lỗi thì thử `.recover`, vẫn lỗi thì dùng bản backup (tạo định kỳ bằng `VACUUM INTO` hoặc backup API), cuối cùng tạo database mới. Thiết bị không được treo vì database hỏng.
- Cấu hình quan trọng nên có bản mặc định trong rootfs read-only.

### 5.4. Chọn journal mode và synchronous

| Cấu hình | Khi nào dùng |
| --- | --- |
| `WAL` + `synchronous=NORMAL` | **Lựa chọn mặc định cho thiết bị IoT**. Database luôn nhất quán, chỉ có thể mất vài giây dữ liệu cuối. Ít `fsync`, ít ghi flash |
| `WAL` + `synchronous=FULL` | Mỗi transaction đã commit phải được giữ: giao dịch thanh toán, bộ đếm điện năng, log sự kiện an toàn |
| `DELETE` + `synchronous=FULL` | Filesystem không hỗ trợ WAL, hoặc database nằm ở nơi nhiều máy cùng truy cập |
| `synchronous=OFF`, `journal_mode=OFF/MEMORY` | Chỉ cho database tạm, mất cũng không sao |

:::warning Lưu ý
`fsync` chỉ đáng tin khi phần cứng tôn trọng lệnh flush. Với thẻ SD giá rẻ, kể cả `synchronous=FULL` cũng không bảo đảm. Sản phẩm cần độ tin cậy cao nên dùng eMMC/SD công nghiệp có power-loss protection, và thử nghiệm rút điện thực tế (cắt nguồn hàng nghìn lần khi đang ghi, sau đó chạy `integrity_check`).
:::

### 5.5. Write amplification

Write amplification là tỷ lệ giữa lượng dữ liệu thực sự ghi xuống flash và lượng dữ liệu ứng dụng muốn ghi. Insert một dòng 30 byte có thể dẫn tới ghi hàng chục KB:

```
INSERT 1 dòng (30 byte)
  -> SQLite: sửa page bảng (4 KB) + page của mỗi index (4 KB mỗi index)
  -> Journal/WAL: ghi thêm bản sao các page
  -> Filesystem: metadata, journal của ext4
  -> FTL: có thể phải xóa và ghi lại cả erase block
  =  có thể 20 - 50 KB ghi thực tế cho 30 byte dữ liệu
```

Cách giảm số lần ghi:

| Cách | Hiệu quả |
| --- | --- |
| Gom nhiều insert vào một transaction (mục 6.1) | Lớn nhất |
| Đệm dữ liệu trong RAM, ghi xuống mỗi N giây | Đổi lại có thể mất dữ liệu trong bộ đệm khi mất điện |
| `WAL` + `synchronous=NORMAL` | Ghi tuần tự, ít `fsync` |
| Giảm số index | Mỗi index thêm ít nhất một page ghi mỗi transaction |
| Tránh `UPDATE` tần suất cao (ví dụ "last_seen" mỗi giây) | Giữ trong RAM, ghi định kỳ |
| `temp_store=MEMORY`, dữ liệu tạm trên tmpfs | Không ghi xuống flash |
| Không lạm dụng `VACUUM` | `VACUUM` ghi lại toàn bộ database |

Đo số byte thực sự ghi xuống:

```bash
grep write_bytes /proc/$(pidof app)/io                       # theo tiến trình
awk '{print $7 * 512 / 1024 " KB"}' /sys/block/mmcblk0/stat  # toàn thiết bị
```

Đo trong một khoảng thời gian rồi quy ra số byte ghi mỗi ngày để ước tính tuổi thọ. Ví dụ: eMMC 8 GB, 3.000 chu kỳ, write amplification của FTL là 3, tổng ghi được khoảng `8 GB x 3000 / 3 = 8 TB`. Ghi 2 GB/ngày thì dùng được khoảng 11 năm, ghi 20 GB/ngày (insert từng dòng không gom transaction) thì chỉ còn khoảng 1 năm.

## 6. Tối ưu hiệu năng

Các tối ưu xếp theo mức độ hiệu quả:
1. Gom ghi vào transaction (mục 6.1).
2. Tái sử dụng prepared statement (mục 6.2).
3. Index đúng với truy vấn, kiểm tra bằng `EXPLAIN QUERY PLAN` (mục 4.4).
4. PRAGMA phù hợp: `WAL`, `synchronous=NORMAL`, `cache_size`, `temp_store` (mục 4.5).
5. Luôn đo trên phần cứng thật (mục 6.3).

### 6.1. Gom ghi vào transaction

Ở chế độ autocommit, mỗi `INSERT` là một transaction riêng với đầy đủ các bước: lấy lock, ghi journal, `fsync`, nhả lock. Tốc độ bị giới hạn bởi số lần `fsync` mỗi giây mà storage làm được, thường chỉ vài chục đến vài trăm lần trên eMMC/SD.

```c
/* Chậm: 10000 transaction, 10000+ lần fsync */
for (int i = 0; i < 10000; i++) {
    bind_values(stmt, i);
    sqlite3_step(stmt);
    sqlite3_reset(stmt);
}

/* Nhanh: 1 transaction, vài lần fsync */
sqlite3_exec(db, "BEGIN IMMEDIATE;", NULL, NULL, NULL);
for (int i = 0; i < 10000; i++) {
    bind_values(stmt, i);
    sqlite3_step(stmt);
    sqlite3_reset(stmt);
}
sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
```

Kết quả tham khảo khi insert 10.000 dòng trên board ARM Cortex-A8 với thẻ SD (chênh lệch cụ thể phụ thuộc phần cứng):

| Cách | Thời gian |
| --- | --- |
| Mỗi insert một transaction, `DELETE` + `FULL` | ~ 3 - 5 phút |
| Mỗi insert một transaction, `WAL` + `NORMAL` | ~ 5 - 15 giây |
| Một transaction cho cả 10.000 dòng | < 1 giây |

Không nên gom quá lớn: transaction càng dài thì giữ lock càng lâu và mất càng nhiều dữ liệu nếu có sự cố. Với collector, gom theo thời gian (mỗi 5 - 10 giây) hoặc theo số mẫu (500 mẫu), tùy điều kiện nào đến trước.

### 6.2. Tái sử dụng prepared statement

Biên dịch SQL tốn CPU (mục 3.3). Với câu lệnh chạy thường xuyên:
- Prepare một lần khi khởi động, giữ `sqlite3_stmt *` suốt thời gian chạy.
- Mỗi lần dùng: `bind` -> `step` -> `reset`.
- Chỉ `finalize` khi tắt chương trình.

```c
struct db_ctx {
    sqlite3 *db;
    sqlite3_stmt *insert_reading;
    sqlite3_stmt *select_latest;
    sqlite3_stmt *mark_sent;
};
```

### 6.3. Benchmark trên phần cứng thật

Kết quả đo trên PC không phản ánh được thiết bị: CPU ARM chậm hơn hàng chục lần, và `fsync` trên eMMC/SD chậm hơn SSD hàng trăm lần.

Chương trình benchmark đơn giản:

```c
/* bench.c */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sqlite3.h>

static double now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

/* batch = số dòng mỗi transaction */
static void bench(sqlite3 *db, int total, int batch)
{
    sqlite3_stmt *stmt;
    sqlite3_exec(db, "DELETE FROM reading;", NULL, NULL, NULL);
    sqlite3_prepare_v2(db, "INSERT INTO reading (sensor_id, ts, value) VALUES (?, ?, ?);",
                       -1, &stmt, NULL);

    double start = now_ms();
    for (int i = 0; i < total; i++) {
        if (i % batch == 0)
            sqlite3_exec(db, "BEGIN;", NULL, NULL, NULL);

        sqlite3_bind_int(stmt, 1, i % 8);
        sqlite3_bind_int64(stmt, 2, 1761465600 + i);
        sqlite3_bind_double(stmt, 3, i * 0.01);
        sqlite3_step(stmt);
        sqlite3_reset(stmt);

        if (i % batch == batch - 1 || i == total - 1)
            sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    }
    double ms = now_ms() - start;

    printf("batch=%-6d rows=%-6d time=%9.1f ms  rate=%9.0f rows/s\n",
           batch, total, ms, total * 1000.0 / ms);
    sqlite3_finalize(stmt);
}

int main(int argc, char *argv[])
{
    const char *path = argc > 1 ? argv[1] : "bench.db";
    const char *mode = argc > 2 ? argv[2] : "WAL";
    const char *sync = argc > 3 ? argv[3] : "NORMAL";
    char sql[128];
    sqlite3 *db;

    sqlite3_open(path, &db);
    snprintf(sql, sizeof(sql), "PRAGMA journal_mode=%s; PRAGMA synchronous=%s;", mode, sync);
    sqlite3_exec(db, sql, NULL, NULL, NULL);
    sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS reading ("
                     "id INTEGER PRIMARY KEY, sensor_id INTEGER, ts INTEGER, value REAL);",
                 NULL, NULL, NULL);

    printf("journal_mode=%s synchronous=%s\n", mode, sync);
    bench(db, 1000, 1);
    bench(db, 10000, 100);
    bench(db, 10000, 10000);

    sqlite3_close(db);
    return 0;
}
```

```bash
arm-linux-gnueabihf-gcc -O2 bench.c sqlite3.c -o bench -lpthread -lm
scp bench root@192.168.7.2:/data/

# Trên thiết bị, chạy trên đúng phân vùng sẽ chứa database
./bench /data/bench.db DELETE FULL
./bench /data/bench.db WAL FULL
./bench /data/bench.db WAL NORMAL
```

Lưu ý khi benchmark:
- Chạy trên đúng storage và filesystem của sản phẩm, không chạy trên tmpfs hay NFS.
- Đo với lượng dữ liệu tương đương sau vài tháng vận hành, không đo với database rỗng.
- Theo dõi cả số byte ghi xuống flash (mục 5.5), không chỉ thời gian.

## 7. Đa luồng và đa tiến trình

Cơ chế lock và `SQLITE_BUSY` đã trình bày ở mục 4.3. Phần này nói về cách tổ chức chương trình.

### 7.1. Threading mode

| Mode | Compile-time | Runtime | Quy tắc |
| --- | --- | --- | --- |
| Single-thread | `SQLITE_THREADSAFE=0` | Không đổi được | Không có mutex, chỉ được dùng SQLite từ một thread |
| Multi-thread | `SQLITE_THREADSAFE=2` | Flag `SQLITE_OPEN_NOMUTEX` | Nhiều thread dùng được, nhưng mỗi connection chỉ được một thread dùng tại một thời điểm |
| Serialized | `SQLITE_THREADSAFE=1` (mặc định) | Flag `SQLITE_OPEN_FULLMUTEX` | Nhiều thread dùng chung một connection an toàn, SQLite tự khóa mutex |

Khuyến nghị: **mỗi thread một connection riêng**. Dùng chung connection thì các thread phải chờ nhau, chung luôn cả transaction, và `sqlite3_errmsg()` của thread này có thể bị lỗi của thread khác ghi đè.

### 7.2. Kiến trúc thường gặp

```
+------------------+      +-------------------+
| Sensor drivers   |      | MQTT / Modbus     |
+--------+---------+      +---------+---------+
         |                          |
         v                          v
+---------------------------------------------+
|  collector  (tiến trình ghi)                |
|  - gom dữ liệu trong RAM                    |
|  - mỗi 5s: BEGIN IMMEDIATE ... COMMIT       |
|  - retention, downsampling, checkpoint      |
+----------------------+----------------------+
                       | ghi
                       v
            +---------------------+
            |  /data/app.db (WAL) |
            +----------+----------+
                       | đọc (SQLITE_OPEN_READONLY)
          +------------+-------------+
          v            v             v
    +----------+ +-----------+ +-----------+
    |  Web UI  | |  REST API | |  Uploader |
    +----------+ +-----------+ +-----------+
```

Nguyên tắc:
- **Một tiến trình ghi duy nhất**. Tiến trình khác muốn thay đổi dữ liệu (web UI đổi cấu hình) thì gửi yêu cầu cho collector qua IPC (Unix socket, D-Bus, MQTT nội bộ). Tránh được xung đột giữa các writer và dễ kiểm soát số lần ghi.
- Reader mở bằng `SQLITE_OPEN_READONLY`, đặt `busy_timeout`, giữ transaction đọc ngắn.
- Dùng `WAL`. Thư mục chứa database phải cho mọi tiến trình truy cập được file `-shm` (chú ý user và quyền khi các service chạy dưới user khác nhau).
- Nếu luôn có reader đang đọc, checkpoint không hoàn thành được và file `-wal` phình to mãi. Collector nên định kỳ gọi `PRAGMA wal_checkpoint(TRUNCATE)` vào lúc ít tải.
- Uploader cần đánh dấu bản ghi đã gửi (mục 8.4) thì cũng là một writer: vẫn ổn với WAL + `BEGIN IMMEDIATE` + `busy_timeout`, hoặc gộp uploader vào tiến trình collector.
- Không mở connection trước `fork()` rồi dùng ở tiến trình con.

## 8. Thiết kế dữ liệu cho IoT

### 8.1. Lưu dữ liệu time-series

**Schema**

Dạng "long" (mỗi dòng một giá trị) linh hoạt khi số lượng và loại cảm biến thay đổi:

```sql
CREATE TABLE sensor (
    id   INTEGER PRIMARY KEY,
    name TEXT NOT NULL UNIQUE,
    unit TEXT NOT NULL
) STRICT;

CREATE TABLE reading (
    id        INTEGER PRIMARY KEY,
    sensor_id INTEGER NOT NULL REFERENCES sensor(id),
    ts        INTEGER NOT NULL,          -- Unix epoch (giây), UTC
    value     REAL    NOT NULL,
    sent      INTEGER NOT NULL DEFAULT 0 -- 0: chưa gửi lên cloud, 1: đã gửi
) STRICT;

CREATE INDEX idx_reading_sensor_ts ON reading(sensor_id, ts);
```

Dạng "wide" (mỗi dòng là một lần lấy mẫu, mỗi cảm biến một cột) nhỏ gọn hơn khi các cảm biến luôn được đọc cùng lúc:

```sql
CREATE TABLE sample (
    ts       INTEGER PRIMARY KEY,        -- mỗi giây tối đa một mẫu
    temp     REAL,
    humidity REAL,
    pressure REAL
) STRICT;
```

Dùng `ts` làm `INTEGER PRIMARY KEY` giúp dữ liệu được sắp xếp theo thời gian ngay trong B-tree chính, không cần index riêng cho `ts`.

**Lưu timestamp: INTEGER hay ISO string?**

| | INTEGER (Unix epoch) | TEXT (ISO 8601) |
| --- | --- | --- |
| Ví dụ | `1761465600` | `'2025-10-26T08:00:00Z'` |
| Dung lượng | 4 - 6 byte (epoch giây), 6 - 8 byte (mili giây) | 20 - 24 byte |
| So sánh, sắp xếp, index | Nhanh, so sánh số nguyên | Chậm hơn, so sánh chuỗi. Chỉ đúng khi mọi giá trị cùng định dạng và cùng múi giờ |
| Tính toán khoảng thời gian | Trừ trực tiếp | Phải chuyển đổi |
| Đọc bằng mắt khi debug | Phải chuyển: `datetime(ts, 'unixepoch')` | Đọc được ngay |

Khuyến nghị: **dùng INTEGER epoch, lưu theo UTC**. Chỉ chuyển sang giờ địa phương khi hiển thị. Chọn đơn vị (giây hay mili giây) theo tần suất lấy mẫu và giữ thống nhất trong toàn hệ thống.

```sql
-- Chuyển đổi khi truy vấn
SELECT datetime(ts, 'unixepoch')              AS utc,
       datetime(ts, 'unixepoch', 'localtime') AS local,
       value
FROM reading ORDER BY ts DESC LIMIT 5;

-- Epoch hiện tại
SELECT unixepoch();                 -- từ bản 3.38
SELECT strftime('%s', 'now');       -- bản cũ hơn, trả về TEXT

-- Dữ liệu trong 24 giờ qua
SELECT * FROM reading WHERE ts >= unixepoch() - 86400;
```

:::warning Lưu ý
Nhiều thiết bị không có RTC hoặc pin RTC hết, sau khi khởi động thời gian là năm 1970 cho đến khi đồng bộ NTP. Dữ liệu ghi trong khoảng này có timestamp sai. Cách xử lý: chờ NTP đồng bộ (`timedatectl show -p NTPSynchronized`) trước khi ghi, hoặc lưu thêm cột `boot_id` + thời gian monotonic để hiệu chỉnh sau, hoặc đánh dấu các bản ghi có `ts` nhỏ hơn thời điểm build firmware là không hợp lệ.
:::

### 8.2. Data retention

Flash có dung lượng giới hạn, database không thể lớn mãi. Cần chính sách xóa dữ liệu cũ.

**Xóa theo thời gian**

```sql
DELETE FROM reading WHERE ts < unixepoch() - 30 * 86400;     -- giữ 30 ngày
```

Xóa một lượng lớn dòng trong một lần giữ lock lâu và tạo transaction lớn. Nên xóa từng đợt nhỏ:

```sql
-- DELETE ... LIMIT chỉ có khi build với SQLITE_ENABLE_UPDATE_DELETE_LIMIT,
-- cách sau dùng được với mọi bản
DELETE FROM reading
WHERE id IN (SELECT id FROM reading WHERE ts < ? ORDER BY id LIMIT 1000);
```

Gọi lặp lại cho đến khi `sqlite3_changes(db)` trả về 0, mỗi đợt là một transaction riêng.

**Mô hình ring buffer**

Giữ tối đa N dòng mới nhất bằng trigger:

```sql
CREATE TRIGGER reading_ring AFTER INSERT ON reading
BEGIN
    DELETE FROM reading WHERE id <= NEW.id - 100000;
END;
```

Hoặc dùng bảng có số slot cố định, ghi đè vòng tròn:

```sql
CREATE TABLE event_log (
    slot INTEGER PRIMARY KEY,         -- 0 .. N-1
    ts   INTEGER NOT NULL,
    msg  TEXT    NOT NULL
) STRICT;

-- seq là bộ đếm tăng dần do ứng dụng giữ, N = 10000
INSERT INTO event_log (slot, ts, msg) VALUES (:seq % 10000, :ts, :msg)
    ON CONFLICT(slot) DO UPDATE SET ts = excluded.ts, msg = excluded.msg;
```

Kích thước file với cách ghi đè slot gần như không đổi theo thời gian.

**Giới hạn dung lượng database**

Khi xóa dòng, page trống được đưa vào freelist và dùng lại cho dữ liệu mới, nhưng **file không tự nhỏ lại**. Kích thước file chỉ giảm khi chạy `VACUUM` hoặc dùng incremental vacuum.

```sql
-- Dung lượng hiện tại và số page trống
SELECT page_count * page_size AS total_bytes,
       freelist_count * page_size AS free_bytes
FROM pragma_page_count(), pragma_page_size(), pragma_freelist_count();

-- Giới hạn cứng: vượt thì INSERT trả về SQLITE_FULL
PRAGMA max_page_count = 25600;       -- 25600 x 4096 = 100 MB

-- Incremental vacuum (phải đặt trước khi tạo bảng, hoặc chạy VACUUM sau khi đặt)
PRAGMA auto_vacuum = INCREMENTAL;
PRAGMA incremental_vacuum(500);      -- trả lại tối đa 500 page trống cho filesystem
```

Chiến lược đề xuất:
- Retention theo thời gian chạy định kỳ (ví dụ mỗi giờ), xóa từng đợt nhỏ.
- Thêm ngưỡng theo dung lượng: nếu `page_count * page_size` vượt 80% giới hạn thì xóa thêm dữ liệu cũ nhất, kể cả khi chưa đủ số ngày.
- Đặt `max_page_count` làm lưới an toàn để database không làm đầy phân vùng (phân vùng đầy có thể làm hỏng các dịch vụ khác).
- Xử lý `SQLITE_FULL` trong code: xóa dữ liệu cũ rồi thử lại.
- Hạn chế `VACUUM` toàn bộ vì ghi lại cả database và cần dung lượng trống gấp đôi.

### 8.3. Downsampling và aggregation

Dữ liệu thô (ví dụ mỗi giây một mẫu) chiếm nhiều dung lượng nhưng sau vài ngày ít khi cần đến độ chi tiết đó. Downsampling gộp dữ liệu theo phút hoặc giờ, giữ lại các giá trị thống kê.

```sql
CREATE TABLE reading_1m (
    sensor_id INTEGER NOT NULL,
    ts        INTEGER NOT NULL,          -- đầu phút
    avg       REAL    NOT NULL,
    min       REAL    NOT NULL,
    max       REAL    NOT NULL,
    count     INTEGER NOT NULL,
    PRIMARY KEY (sensor_id, ts)
) STRICT, WITHOUT ROWID;
```

Gộp dữ liệu của một khoảng thời gian (chạy định kỳ, ví dụ mỗi phút cho phút vừa kết thúc):

```sql
INSERT INTO reading_1m (sensor_id, ts, avg, min, max, count)
SELECT sensor_id, ts / 60 * 60, AVG(value), MIN(value), MAX(value), COUNT(*)
FROM reading
WHERE ts >= :from AND ts < :to
GROUP BY sensor_id, ts / 60
ON CONFLICT(sensor_id, ts) DO UPDATE SET
    avg   = excluded.avg,
    min   = excluded.min,
    max   = excluded.max,
    count = excluded.count;
```

- `ts / 60 * 60`: phép chia số nguyên làm tròn xuống đầu phút. Theo giờ dùng `ts / 3600 * 3600`.
- `ON CONFLICT ... DO UPDATE` cho phép chạy lại cùng khoảng thời gian (ví dụ khi có dữ liệu đến trễ) mà không bị trùng.
- `WHERE ts >= :from AND ts < :to` tận dụng được index, không dùng hàm trên cột `ts`.

Retention nhiều tầng:

| Bảng | Độ phân giải | Thời gian giữ |
| --- | --- | --- |
| `reading` | Dữ liệu thô | 7 ngày |
| `reading_1m` | 1 phút | 90 ngày |
| `reading_1h` | 1 giờ | 2 năm |

Web UI chọn bảng phù hợp theo khoảng thời gian người dùng xem: xem 1 giờ dùng dữ liệu thô, xem 1 tháng dùng bảng theo giờ. Truy vấn nhanh hơn nhiều và trả về ít điểm hơn.

### 8.4. Store-and-forward

Thiết bị IoT thường mất kết nối mạng: 4G yếu, Wi-Fi chập chờn, server bảo trì. Store-and-forward là mô hình lưu dữ liệu vào SQLite trước, sau đó một tiến trình riêng gửi lên cloud khi có mạng và chỉ đánh dấu đã gửi khi server xác nhận.

```
 Sensor --> [collector] --INSERT sent=0--> +-----------+
                                           |  SQLite   |
                                           +-----------+
                                             |      ^
                     SELECT ... WHERE sent=0 |      | UPDATE sent=1
                       ORDER BY id LIMIT 100 |      | WHERE id IN (...)
                                             v      |
                                         [uploader] -+
                                             |      ^
                                     PUBLISH |      | PUBACK (QoS 1)
                                     QoS 1   v      | / HTTP 200
                                         +-----------+
                                         |   Cloud   |
                                         +-----------+
```

Partial index giúp tìm các bản ghi chưa gửi nhanh và nhỏ (chỉ chứa dòng `sent = 0`):

```sql
CREATE INDEX idx_reading_unsent ON reading(id) WHERE sent = 0;
```

Vòng lặp uploader:

```sql
-- 1. Lấy một lô chưa gửi, cũ nhất trước
SELECT id, sensor_id, ts, value FROM reading
WHERE sent = 0 ORDER BY id LIMIT 100;

-- 2. Gửi lô đó lên cloud (MQTT QoS 1 hoặc HTTP POST)

-- 3. Chỉ sau khi nhận PUBACK / HTTP 2xx: đánh dấu đã gửi
UPDATE reading SET sent = 1 WHERE sent = 0 AND id <= :last_id_in_batch;
```

Những điểm cần chú ý:
- **At-least-once**: nếu mất điện sau khi server nhận nhưng trước khi `UPDATE`, lô đó sẽ được gửi lại. Gửi kèm `id` bản ghi (hoặc cặp `device_id` + `id`) để server loại bỏ bản trùng.
- Gom nhiều bản ghi vào một message (một JSON array) giảm overhead mạng và số lần `UPDATE`.
- Với MQTT, đánh dấu `sent` trong callback `on_publish` (QoS 1 là khi nhận PUBACK), không đánh dấu ngay sau khi gọi `mosquitto_publish()` vì hàm này chỉ đưa gói vào hàng đợi. Xem bài MQTT trong mục Network.
- Khi mất mạng lâu, dữ liệu chưa gửi tích lũy. Chính sách retention phải quyết định: có xóa bản ghi chưa gửi khi database gần đầy hay không. Thông thường ưu tiên xóa bản ghi đã gửi trước.
- Khi có mạng lại, gửi theo lô với tốc độ giới hạn để không làm nghẽn đường truyền và không làm chậm dữ liệu realtime.

Có thể tách thành bảng `outbox` riêng thay vì cột `sent`: collector insert vào cả `reading` và `outbox`, uploader gửi xong thì `DELETE FROM outbox`. Bảng `outbox` luôn nhỏ, truy vấn nhanh, và retention của `reading` không bị ảnh hưởng bởi trạng thái gửi.

### 8.5. JSON1 extension

Từ bản 3.38.0, các hàm JSON được tích hợp sẵn (trước đó cần bật `SQLITE_ENABLE_JSON1`). JSON được lưu dạng `TEXT`, tiện cho payload có cấu trúc thay đổi theo loại thiết bị.

```sql
CREATE TABLE event (
    id      INTEGER PRIMARY KEY,
    ts      INTEGER NOT NULL,
    type    TEXT    NOT NULL,
    payload TEXT    NOT NULL CHECK (json_valid(payload))
) STRICT;

INSERT INTO event (ts, type, payload) VALUES
    (unixepoch(), 'alarm', '{"code": 12, "level": "high", "zones": [1, 3]}'),
    (unixepoch(), 'door',  '{"state": "open", "user": "admin"}');
```

Các hàm thường dùng:

```sql
-- Lấy giá trị theo đường dẫn
SELECT json_extract(payload, '$.code') FROM event WHERE type = 'alarm';
SELECT payload ->> '$.level' FROM event;        -- ->> trả về giá trị SQL (từ 3.38)
SELECT payload -> '$.zones' FROM event;         -- -> trả về JSON

-- Sửa một trường
UPDATE event SET payload = json_set(payload, '$.ack', 1) WHERE id = 1;

-- Duyệt phần tử của mảng
SELECT e.id, z.value AS zone
FROM event e, json_each(e.payload, '$.zones') z
WHERE e.type = 'alarm';

-- Tạo JSON từ dữ liệu bảng (tiện để gửi lên cloud)
SELECT json_group_array(json_object('ts', ts, 'v', value))
FROM reading WHERE sent = 0 LIMIT 100;
```

Truy vấn thường xuyên theo một trường JSON thì tạo generated column và index trên đó:

```sql
ALTER TABLE event ADD COLUMN level TEXT
    GENERATED ALWAYS AS (payload ->> '$.level') VIRTUAL;
CREATE INDEX idx_event_level ON event(level);
```

Không nên lưu mọi thứ thành JSON: các trường truy vấn và lọc thường xuyên (timestamp, sensor_id, giá trị đo) nên là cột thật để tận dụng kiểu dữ liệu và index. JSON phù hợp với phần dữ liệu phụ, ít truy vấn.

### 8.6. Lưu cấu hình thiết bị bằng SQLite

Cấu hình thường được lưu trong file text (INI, JSON, YAML). Vấn đề là ghi file text không atomic: mất điện giữa lúc ghi có thể để lại file rỗng hoặc ghi dở, thiết bị khởi động lên với cấu hình hỏng. Cách đúng với file text là ghi ra file tạm, `fsync`, rồi `rename()`, nhưng dễ làm sai.

Dùng SQLite cho cấu hình:
- Cập nhật nhiều tham số trong một transaction: hoặc tất cả được áp dụng, hoặc không có gì thay đổi.
- Không cần tự viết parser.
- Có thể lưu thêm metadata: thời điểm sửa, ai sửa, giá trị mặc định.
- Dùng chung database với dữ liệu, hoặc tách file riêng (`config.db`) để cấu hình không bị ảnh hưởng bởi dữ liệu cảm biến ghi liên tục. Nên tách riêng.

```sql
CREATE TABLE config (
    key        TEXT PRIMARY KEY,
    value      ANY  NOT NULL,
    updated_at INTEGER NOT NULL DEFAULT (unixepoch())
) STRICT, WITHOUT ROWID;

INSERT INTO config (key, value) VALUES
    ('mqtt.host', 'broker.local'),
    ('mqtt.port', 1883),
    ('sample.interval_ms', 1000),
    ('retention.days', 30);

-- Cập nhật nhiều tham số atomic
BEGIN IMMEDIATE;
INSERT INTO config (key, value) VALUES ('mqtt.host', '10.0.0.5')
    ON CONFLICT(key) DO UPDATE SET value = excluded.value, updated_at = unixepoch();
INSERT INTO config (key, value) VALUES ('mqtt.port', 8883)
    ON CONFLICT(key) DO UPDATE SET value = excluded.value, updated_at = unixepoch();
COMMIT;
```

Cột kiểu `ANY` trong bảng `STRICT` giữ nguyên kiểu của giá trị (`1883` là INTEGER, `'broker.local'` là TEXT), không bị chuyển đổi như affinity thông thường. Dùng `sqlite3_column_type()` để biết kiểu khi đọc.

**Quản lý phiên bản schema**

Firmware mới có thể cần thêm bảng hoặc thêm cột. `PRAGMA user_version` là một số nguyên lưu trong header file database, dùng để đánh dấu phiên bản schema:

```c
static const char *MIGRATIONS[] = {
    /* v1 */ "CREATE TABLE config (key TEXT PRIMARY KEY, value ANY NOT NULL) STRICT, WITHOUT ROWID;",
    /* v2 */ "ALTER TABLE config ADD COLUMN updated_at INTEGER NOT NULL DEFAULT 0;",
    /* v3 */ "CREATE TABLE event (id INTEGER PRIMARY KEY, ts INTEGER NOT NULL, payload TEXT) STRICT;",
};

static int migrate(sqlite3 *db)
{
    sqlite3_stmt *stmt;
    int version = 0;
    int target = sizeof(MIGRATIONS) / sizeof(MIGRATIONS[0]);

    sqlite3_prepare_v2(db, "PRAGMA user_version;", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW)
        version = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    /* Mỗi bước nâng cấp là một transaction, lỗi thì giữ nguyên phiên bản cũ */
    for (; version < target; version++) {
        char sql[64];
        snprintf(sql, sizeof(sql), "PRAGMA user_version = %d;", version + 1);

        if (sqlite3_exec(db, "BEGIN IMMEDIATE;", NULL, NULL, NULL) != SQLITE_OK ||
            sqlite3_exec(db, MIGRATIONS[version], NULL, NULL, NULL) != SQLITE_OK ||
            sqlite3_exec(db, sql, NULL, NULL, NULL) != SQLITE_OK ||
            sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL) != SQLITE_OK) {
            fprintf(stderr, "Migration to v%d failed: %s\n", version + 1, sqlite3_errmsg(db));
            sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
            return -1;
        }
        printf("Database migrated to v%d\n", version + 1);
    }
    return 0;
}
```

Khi cập nhật firmware OTA, database trên phân vùng dữ liệu được giữ lại và tự nâng cấp ở lần chạy đầu tiên của firmware mới. Nếu hệ thống có cơ chế rollback firmware (A/B), firmware cũ phải chịu được database đã nâng cấp: chỉ nên thêm bảng, thêm cột có giá trị mặc định, tránh xóa hay đổi tên cột.
