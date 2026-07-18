## 1. Khái niệm cơ bản

### Repository

Là nơi Git lưu toàn bộ lịch sử của dự án. Khi chạy `git init`, Git tạo thư mục ẩn `.git/` chứa toàn bộ database gồm commit, branch, tag, config... Có 2 loại:
- **Local repo**: nằm trên máy, làm việc trực tiếp.
- **Remote repo**: nằm trên server (GitHub, GitLab...), dùng để chia sẻ và đồng bộ giữa các thành viên trong project.

Một số thành phần trong `.git/`:

| Đường dẫn | Chứa gì |
|---|---|
| `.git/objects/` | Database chính: nội dung file, cây thư mục, commit |
| `.git/refs/heads/` | Các branch local (mỗi branch là 1 file chứa hash commit) |
| `.git/refs/remotes/` | Vị trí các remote branch lần cuối fetch về |
| `.git/HEAD` | Cho biết đang đứng ở branch/commit nào |
| `.git/index` | Staging area |
| `.git/config` | Cấu hình riêng của repo |

Xoá thư mục `.git/` đồng nghĩa với xoá toàn bộ lịch sử

Ngoài ra còn khái niệm **bare repo** (`git init --bare`): repo chỉ có phần dữ liệu `.git/` mà không có working directory, dùng làm repo trung tâm trên server để mọi người push/pull.

### Ba khu vực làm việc

Đây là mô hình quan trọng nhất của Git. Một file di chuyển qua 3 khu vực:
- **Working directory**: thư mục dự án thực tế, nơi ta chỉnh sửa file.
- **Staging area** (còn gọi là *index*): khu vực trung gian chứa các thay đổi được chọn để đưa vào commit tiếp theo. Nhờ có staging area, ta có thể sửa 10 file nhưng chỉ commit 3 file liên quan đến nhau.
- **Repository**: nơi lưu các commit, snapshot vĩnh viễn của dự án.

```
Working Directory  --git add-->  Staging Area  --git commit-->  Repository
```

**Vì sao cần staging area?** Nó cho phép ta soạn commit một cách chủ động: gom các thay đổi liên quan thành một commit có ý nghĩa, thay vì commit tất cả những gì đang dở dang. Commit nhỏ, tập trung vào một việc thì dễ review, dễ revert.

:::tip Ghi chú
Một file vừa sửa sau khi `git add` có thể xuất hiện ở cả hai trạng thái: phần đã add nằm ở staging, phần sửa tiếp sau đó nằm ở working directory. `git status` khi đó liệt kê file ở cả hai mục.
:::

### Trạng thái của file

- **Untracked**: file mới tạo, Git chưa theo dõi. Không bị ảnh hưởng bởi commit/checkout, nhưng cũng không được Git bảo vệ, xoá là mất.
- **Tracked**: file Git đang theo dõi, gồm 3 trạng thái con:
  - **Unmodified**: chưa thay đổi so với commit gần nhất.
  - **Modified**: đã sửa nhưng chưa `git add`.
  - **Staged**: đã `git add`, sẵn sàng để commit.
- **Ignored**: file match pattern trong `.gitignore` -> Git chủ động bỏ qua, không hiện trong `git status`

### Commit

Một snapshot của toàn bộ dự án tại một thời điểm. Mỗi commit chứa:

- **Tree**: con trỏ đến snapshot của cây thư mục tại thời điểm commit.
- **Parent**: hash của commit cha.
- **Author / Committer**: tên, email, thời gian.
- **Message**: mô tả thay đổi.

Toàn bộ nội dung trên được băm thành mã SHA-1 duy nhất (ví dụ `2cece91...`). Vì hash được tính từ nội dung + hash của cha nên sửa bất kỳ commit nào trong quá khứ sẽ làm mọi commit sau nó đổi hash. Đó là lý do amend/rebase được gọi là viết lại lịch sử.

Các commit nối với nhau qua con trỏ parent tạo thành chuỗi lịch sử:

```
A <--- B <--- C <--- D   (mỗi commit trỏ về cha của nó)
```

### Branch

Là một con trỏ di động trỏ đến một commit. Thực chất chỉ là một file 41 byte trong `.git/refs/heads/` chứa hash. Khi ta commit, con trỏ của branch hiện tại tự tiến lên commit mới:

```
              main
               |
A --- B --- C --- D
               \
                E --- F
                      |
                   feature
```

Vì branch chỉ là con trỏ nên tạo/xoá branch gần như tức thời (không copy code), cho phép phát triển tính năng song song mà không ảnh hưởng nhánh chính. Quy ước phổ biến: nhánh chính luôn ở trạng thái ổn định. Mỗi tính năng/bugfix làm trên một branch riêng (`feature/login`, `fix/null-pointer`...) rồi merge về.

### HEAD

Con trỏ cho biết ta đang đứng ở đâu. Bình thường HEAD trỏ đến một branch chứ không trỏ thẳng đến commit và branch mới trỏ đến commit:

```
HEAD ---> main ---> commit D
```

Nhờ cơ chế gián tiếp này, khi ta commit thì branch tiến lên còn HEAD vẫn bám theo branch.

Khi checkout thẳng một commit hoặc tag (`git checkout 2cece91`), HEAD trỏ trực tiếp đến commit — trạng thái **detached HEAD**:

```
HEAD ---> commit C        (không qua branch nào)
```

Lúc này vẫn xem code, build, thậm chí commit được, nhưng commit mới tạo không thuộc branch nào — chuyển đi nơi khác là chúng thành orphan và sẽ bị dọn rác sau này. Muốn giữ lại thì tạo branch ngay tại đó: `git switch -c <tên-branch>`.

### Stash

Stash dùng để cất các thay đổi chưa commit, đưa working directory về trạng thái sạch. Dùng khi đang làm dở mà cần chuyển branch gấp -> cất đi, làm việc khác, rồi lấy lại sau.

Stash hoạt động như một stack: cất mới nhất nằm trên cùng là `stash@{0}`, cái trước đó bị đẩy xuống `stash@{1}`... Lấy ra bằng `pop` (lấy và xoá khỏi stack) hoặc `apply` (lấy nhưng vẫn giữ trong stack). Mặc định stash chỉ cất các thay đổi của file tracked; file untracked cần thêm cờ `-u`.

Lưu ý stash là cơ chế thuần local, không push lên remote được và các stash để lâu rất dễ quên. Nếu công việc dở dang có giá trị, tạo branch và commit tạm (`wip:`) thường an toàn hơn.

### Remote, fetch, pull, push

- **Remote**: tên đại diện cho một repo ở xa, mặc định là `origin`. Một repo có thể có nhiều remote, ví dụ khi fork, ta thường có `origin` (fork của mình) và `upstream` (repo gốc).
- **Fetch**: tải commit mới từ remote về nhưng không thay đổi code đang làm.
- **Pull**: fetch + merge hoặc rebase vào branch hiện tại.
- **Push**: đẩy commit từ local lên remote. Chỉ thành công khi lịch sử local chứa lịch sử remote. Nếu remote có commit mình chưa có, phải pull về trước.

### Merge vs Rebase

Giả sử `feature` tách ra từ `main` và cả hai đều có commit mới:

```
    A --- B --- C       (main)
            \
            D --- E     (feature)
```

- **Merge:** tạo một commit gộp có 2 cha, giữ nguyên lịch sử hai nhánh, không viết lại gì, nhưng nhiều nhánh đan xen làm lịch sử khó đọc:

```
        A --- B --- C --- M    (main)
               \         /
                D --- E        (feature)
```

- **Rebase:** nhấc các commit của nhánh mình đặt lên đỉnh nhánh kia. D, E được tạo lại thành D', E' -> lịch sử thẳng, đẹp:

```
        A --- B --- C          (main)
                     \
                      D' --- E'   (feature)
```

**Viết lại lịch sử và mất commit nghĩa là gì?**

Hai cụm từ này xuất hiện ở khắp nơi nên cần hiểu chính xác.

Git không bao giờ sửa hay xoá commit đã tạo. Commit là bất biến, sửa một commit (amend, rebase) thực chất là tạo commit mới với nội dung mới, rồi dời con trỏ branch sang chuỗi commit mới. Chuỗi commit cũ vẫn nằm nguyên trong `.git/objects/`:

```
Trước amend:              Sau amend:
                                    C   (commit cũ — vẫn tồn tại, nhưng không branch nào trỏ tới)
A --- B --- C             A --- B <
            |                       C'  (commit mới)
           main                     |
                                   main
```

**Mất commit = commit không còn đường nào dẫn tới.** Git tìm commit bằng cách đi từ các con trỏ (branch, tag, HEAD) lần ngược theo parent. Commit nào không nằm trên đường đi từ bất kỳ con trỏ nào như `C` ở hình trên sẽ trở thành **unreachable**: `git log` không hiện, coi như mất. Nhưng nó chưa bị xoá thật:

- **Reflog** ghi lại mọi vị trí HEAD từng đứng, nên vẫn tìm lại được hash của `C` và cứu bằng `git branch recover <hash>`.
- Chỉ khi reflog hết hạn (mặc định ~90 ngày, với commit unreachable là ~30 ngày) và `git gc` chạy dọn rác, commit mới bị xoá vật lý.

Vậy nên khi lỡ tay `reset --hard`, rebase hỏng, xoá nhầm branch thì bình tĩnh mở `git reflog` thì sẽ có thể cứu được. Thứ không cứu được là các thay đổi chưa từng commit, chúng chưa bao giờ vào database của Git.

### Conflict

Xảy ra khi hai nhánh cùng sửa một đoạn code và Git không tự quyết được. Git dừng lại, đánh dấu trong file:

```
<<<<<<< HEAD
int timeout = 30;              // phiên bản của nhánh hiện tại
=======
int timeout = 60;              // phiên bản của nhánh đang merge vào
>>>>>>> feature
```

Ta phải tự sửa tay: chọn một bên hoặc viết lại hoặc kết hợp cả hai, rồi xoá hết các dòng đánh dấu. Sau đó `git add <file>` để báo đã giải quyết và kết thúc bằng `git commit` với merge hoặc `git rebase --continue` với rebase. Muốn bỏ giữa chừng: `git merge --abort` hoặc `git rebase --abort` để đưa mọi thứ về như cũ.

## 2. Cấu hình (git config)

| Lệnh | Ý nghĩa |
|---|---|
| `git config --global user.name "Tên"` | Đặt tên tác giả |
| `git config --global user.email "mail"` | Đặt email tác giả |
| `git config --global core.editor vim` | Đặt editor mặc định |
| `git config --global init.defaultBranch main` | Tên branch mặc định khi init |
| `git config --list` | Xem toàn bộ cấu hình |
| `git config user.name` | Xem một giá trị cụ thể |

Phạm vi cấu hình:
- `--system`: toàn máy (`/etc/gitconfig`)
- `--global`: toàn user (`~/.gitconfig`)
- không có cờ: chỉ repo hiện tại (`.git/config`)

Một số alias hữu ích:

```bash
git config --global alias.st status
git config --global alias.co checkout
git config --global alias.lg "log --oneline --graph --all"
```

## 3. Khởi tạo repo (init, clone)

| Lệnh | Ý nghĩa |
|---|---|
| `git init` | Tạo repo mới tại thư mục hiện tại |
| `git init --bare` | Tạo bare repo (làm server, không có working directory) |
| `git clone <url>` | Tải repo về máy |
| `git clone <url> <folder>` | Clone vào thư mục chỉ định |
| `git clone -b <branch> <url>` | Clone và checkout branch cụ thể |
| `git clone --depth 1 <url>` | Shallow clone — chỉ lấy commit mới nhất (nhanh, nhẹ, hữu ích với repo lớn như linux kernel) |

## 4. Theo dõi thay đổi (status, add, diff)

| Lệnh | Ý nghĩa |
|---|---|
| `git status` | Xem trạng thái các file |
| `git status -s` | Dạng rút gọn |
| `git add <file>` | Đưa file thay đổi vào staging area |
| `git add .` | Add tất cả file thay đổi trong thư mục hiện tại vào staging area |
| `git add -A` | Add tất cả file thay đổi trong toàn repo vào staging area |
| `git add -p` | Add từng đoạn thay đổi, chọn có/không cho từng đoạn |
| `git diff` | So sánh working directory với staging area |
| `git diff --staged` | So sánh staging area với commit gần nhất |
| `git diff HEAD` | So sánh working directory với commit gần nhất |
| `git diff <commit1> <commit2>` | So sánh 2 commit |
| `git diff <branch1>..<branch2>` | So sánh 2 branch |
| `git diff --stat` | Chỉ xem thống kê số dòng thay đổi |

Sơ đồ phạm vi của `git diff`:

```
Working Directory    Staging Area    Last Commit (HEAD)
       |------ git diff -----|             |
       |                     |--- git diff staged ---|
       |------------ git diff HEAD --------|
```

## 5. Commit

| Lệnh | Ý nghĩa |
|---|---|
| `git commit -m "message"` | Commit với message |
| `git commit -am "message"` | Add các file đã tracked + commit |
| `git commit --amend` | Sửa commit gần nhất (message hoặc thêm file đã staged) |
| `git commit --amend --no-edit` | Thêm thay đổi vào commit gần nhất, giữ nguyên message |
| `git commit --allow-empty -m "msg"` | Commit rỗng (thường để trigger CI) |

:::warning Lưu ý
`--amend` viết lại commit (hash thay đổi). Không amend commit đã push chung với người khác. Nếu buộc phải push, cần `git push --force-with-lease`.
:::

## 6. Conventional Commits

```
<type>(<scope>): <subject>

<body>

<footer>
```

`Type` là phần bắt buộc, cho biết loại thay đổi:
- feat: thêm tính năng mới
- fix: sửa bug
- docs: chỉ thay đổi tài liệu
- style: format code, thiếu dấu chấm phẩy… (không ảnh hưởng logic)
- refactor: tái cấu trúc code, không thêm feature hay sửa bug
- perf: cải thiện hiệu năng
- test: thêm hoặc sửa test
- build: thay đổi hệ thống build, dependencies (ví dụ: npm, gradle)
- ci: thay đổi CI/CD config (GitHub Actions, Jenkins…)
- chore: việc lặt vặt khác (cập nhật .gitignore, tooling…)
- revert: revert một commit trước đó

`Scope` (tuỳ chọn) là phạm vi ảnh hưởng, đặt trong ngoặc đơn. Ví dụ: auth, api, boot, ui, database...Tuỳ dự án tự quy ước.

`Subject` là mô tả ngắn gọn, không viết hoa chữ đầu, không chấm cuối.

Một số ví dụ thực tế:

```
feat(auth): add Google OAuth2 login
fix(api): handle null response from payment gateway
docs(readme): update installation instructions
refactor(cart): extract price calculation into separate service
build(deps): bump express from 4.18 to 4.19
ci(github): add automated release workflow
perf(query): add index on users.email column
feat!: drop support for Node 14
```

Dấu `!` sau type/scope (như `feat!:`) báo hiệu đây là breaking change — thay đổi không tương thích ngược.

Body và footer dùng khi cần giải thích thêm:

```
fix(payment): prevent duplicate charge on retry

The retry logic was not checking if the previous
transaction had already succeeded, causing double
charges for ~2% of retries.

Closes #1234
```

Footer thường chứa `Closes #issue`, `BREAKING CHANGE: mô tả` hoặc reference đến ticket.

## 7. Xem lịch sử (log, show, blame)

| Lệnh | Ý nghĩa |
|---|---|
| `git log` | Xem lịch sử commit |
| `git log --oneline` | Mỗi commit một dòng |
| `git log --oneline --graph --all` | Vẽ đồ thị branch |
| `git log -p` | Kèm nội dung diff của từng commit |
| `git log -n 5` | Chỉ xem 5 commit gần nhất |
| `git log --author="tên"` | Lọc theo tác giả |
| `git log --since="2 weeks ago"` | Lọc theo thời gian |
| `git log --grep="keyword"` | Tìm trong commit message |
| `git log -- <file>` | Lịch sử của một file |
| `git log --follow -- <file>` | Lịch sử của file, theo cả khi bị rename |
| `git show <commit>` | Xem chi tiết một commit |
| `git show <commit>:<file>` | Xem nội dung file tại một commit |
| `git blame <file>` | Xem ai sửa từng dòng, ở commit nào |
| `git shortlog -sn` | Đếm số commit theo từng tác giả |

Cú pháp tham chiếu commit hay dùng:

| Cú pháp | Ý nghĩa |
|---|---|
| `HEAD` | Commit hiện tại |
| `HEAD~1` (hoặc `HEAD~`) | Commit cha |
| `HEAD~3` | Lùi 3 commit |
| `HEAD^2` | Cha thứ 2 của merge commit |
| `<branch>@{2}` | Vị trí của branch 2 lần thay đổi trước (theo reflog) |

## 8. Branch

| Lệnh | Ý nghĩa |
|---|---|
| `git branch` | Liệt kê branch local |
| `git branch -a` | Liệt kê cả branch remote |
| `git branch -v` | Kèm commit gần nhất của mỗi branch |
| `git branch <name>` | Tạo branch mới (không chuyển sang) |
| `git switch <name>` | Chuyển branch |
| `git switch -c <name>` | Tạo và chuyển sang branch mới |
| `git switch -` | Quay lại branch trước đó |
| `git branch -m <old> <new>` | Đổi tên branch |
| `git branch -d <name>` | Xoá branch (đã merge) |
| `git branch -D <name>` | Xoá branch bắt buộc (chưa merge) |
| `git branch --merged` | Liệt kê branch đã merge vào branch hiện tại |
| `git checkout <name>` | Chuyển branch |
| `git checkout -b <name>` | Tạo và chuyển sang branch mới, tương đương với `git switch -c` |
| `git checkout <commit>` | Checkout một commit cụ thể (detached HEAD) |

:::tip Ghi chú
`git checkout` gánh quá nhiều nhiệm vụ (chuyển branch, khôi phục file...) nên Git 2.23+ tách thành `git switch` (chuyển branch) và `git restore` (khôi phục file). Nên dùng lệnh mới.
:::

## 9. Merge và Rebase

| Lệnh | Ý nghĩa |
|---|---|
| `git merge <branch>` | Merge branch chỉ định vào branch hiện tại |
| `git merge --no-ff <branch>` | Luôn tạo merge commit (kể cả khi fast-forward được) |
| `git merge --squash <branch>` | Gộp toàn bộ commit của branch thành một thay đổi, tự commit sau |
| `git merge --abort` | Huỷ merge đang conflict |
| `git rebase <branch>` | Đặt các commit của branch hiện tại lên đỉnh branch chỉ định |
| `git rebase -i HEAD~3` | Rebase tương tác 3 commit gần nhất (sửa/gộp/xoá/sắp xếp commit) |
| `git rebase --continue` | Tiếp tục sau khi sửa xong conflict |
| `git rebase --abort` | Huỷ rebase, quay về trạng thái ban đầu |
| `git cherry-pick <commit>` | Lấy một commit từ branch khác áp vào branch hiện tại |

Quy trình xử lý conflict:

```bash
git merge feature          # báo conflict
# 1. Mở file bị conflict, sửa các đoạn <<<<<<< ======= >>>>>>>
# 2. Đánh dấu đã giải quyết:
git add <file>
# 3. Hoàn tất:
git commit                 # với merge
git rebase --continue      # với rebase
```

Các action trong rebase tương tác (`git rebase -i`):

| Action | Ý nghĩa |
|---|---|
| `pick` | Giữ nguyên commit |
| `reword` | Sửa message |
| `edit` | Dừng lại để sửa nội dung commit |
| `squash` | Gộp vào commit trước, giữ cả 2 message |
| `fixup` | Gộp vào commit trước, bỏ message |
| `drop` | Xoá commit |

:::warning Lưu ý
Không rebase hoặc amend các commit đã push lên branch chung. Rebase viết lại lịch sử, người khác đã pull về sẽ bị lệch lịch sử.
:::

## 10. Remote (fetch, pull, push)

| Lệnh | Ý nghĩa |
|---|---|
| `git remote -v` | Liệt kê remote và URL |
| `git remote add origin <url>` | Thêm remote |
| `git remote set-url origin <url>` | Đổi URL remote |
| `git remote remove <name>` | Xoá remote |
| `git fetch` | Tải commit mới từ remote (không merge) |
| `git fetch --prune` | Fetch và xoá các branch remote đã bị xoá trên server |
| `git pull` | Fetch + merge vào branch hiện tại |
| `git pull --rebase` | Fetch + rebase (lịch sử thẳng, không có merge commit) |
| `git push` | Đẩy commit lên remote |
| `git push -u origin <branch>` | Push lần đầu, thiết lập tracking (`-u` = `--set-upstream`) |
| `git push --force-with-lease` | Force push an toàn (từ chối nếu remote có commit mới của người khác) |
| `git push origin --delete <branch>` | Xoá branch trên remote |
| `git push --tags` | Đẩy tất cả tag lên remote |

:::warning Lưu ý
Ưu tiên `--force-with-lease` thay vì `--force`: nó kiểm tra remote chưa có commit mới trước khi ghi đè, tránh xoá nhầm công sức của người khác.
:::

## 11. Stash

| Lệnh | Ý nghĩa |
|---|---|
| `git stash` | Cất các thay đổi tracked (working + staged) |
| `git stash -u` | Cất cả file untracked |
| `git stash push -m "message"` | Cất kèm ghi chú |
| `git stash push <file>` | Chỉ cất một file cụ thể |
| `git stash list` | Liệt kê các stash (`stash@{0}` là mới nhất) |
| `git stash show -p stash@{0}` | Xem nội dung một stash |
| `git stash apply` | Lấy stash mới nhất ra, giữ stash trong danh sách |
| `git stash pop` | Lấy stash mới nhất ra và xoá khỏi danh sách |
| `git stash apply stash@{2}` | Lấy stash cụ thể |
| `git stash drop stash@{0}` | Xoá một stash |
| `git stash clear` | Xoá tất cả stash |
| `git stash branch <name>` | Tạo branch mới từ stash (hữu ích khi apply bị conflict) |

Tình huống điển hình:

```bash
# Đang làm dở trên feature, cần fix bug gấp trên main
git stash push -m "dang lam do form login"
git switch main
# ... fix bug, commit ...
git switch feature
git stash pop
```

## 12. Hoàn tác thay đổi (restore, reset, revert)

Chọn lệnh theo tình huống:

| Muốn hoàn tác | Lệnh |
|---|---|
| Thay đổi ở working directory (chưa add) | `git restore <file>` |
| Bỏ file khỏi staging (đã add, chưa commit) | `git restore --staged <file>` |
| Lấy lại file từ một commit cũ | `git restore --source=<commit> <file>` |
| Sửa commit gần nhất | `git commit --amend` |
| Bỏ commit local, giữ thay đổi | `git reset --soft HEAD~1` |
| Bỏ commit local, xoá sạch thay đổi | `git reset --hard HEAD~1` |
| Hoàn tác commit đã push | `git revert <commit>` |

### git reset — 3 chế độ

`git reset <commit>` kéo branch hiện tại về commit chỉ định. Ba chế độ khác nhau ở chỗ giữ lại gì:

| Chế độ | Staging area | Working directory |
|---|---|---|
| `--soft` | Giữ (các thay đổi thành staged) | Giữ |
| `--mixed` (mặc định) | Bỏ (thành modified) | Giữ |
| `--hard` | Bỏ | Xoá sạch |

```bash
git reset --soft HEAD~1     # gỡ commit cuối, thay đổi vẫn staged
git reset HEAD~1            # gỡ commit cuối, thay đổi về working directory
git reset --hard HEAD~1     # gỡ commit cuối, mất luôn thay đổi
git reset --hard origin/main  # đưa local về đúng trạng thái remote
```

:::warning Lưu ý
`git reset --hard` xoá vĩnh viễn các thay đổi chưa commit — không lấy lại được. Với commit đã mất do reset, còn cứu được qua `git reflog` (xem [mục 15](#15-tìm-kiếm-và-gỡ-lỗi-grep-bisect-reflog)).
:::

### git revert — hoàn tác an toàn

Tạo một commit mới đảo ngược thay đổi của commit cũ, không viết lại lịch sử — an toàn cho branch đã push chung:

```bash
git revert <commit>           # revert một commit
git revert HEAD               # revert commit gần nhất
git revert --no-commit <c1> <c2>   # revert nhiều commit, tự commit một lần
```

## 13. Xoá file untracked (clean)

File untracked là các file chưa được git theo dõi.

| Lệnh | Ý nghĩa |
|---|---|
| `git clean -n` | Xem trước sẽ xoá gì (dry-run, **nên chạy trước**) |
| `git clean -f` | Xoá file untracked |
| `git clean -fd` | Xoá cả folder untracked |
| `git clean -fx` | Xoá cả file trong `.gitignore` (build output...) |
| `git clean -fd -e <file>` | Xoá nhưng giữ lại file/folder chỉ định |

Để xoá các file này, ta sử dụng lệnh sau:

```bash
git clean -f
```

:::warning Lưu ý
Các file trong `.gitignore` sẽ không bị xóa khi dùng `git clean -f` (muốn xoá cả chúng phải thêm `-x`). File bị `git clean` xoá không nằm trong lịch sử Git nên không khôi phục được — luôn chạy `git clean -n` trước để xem danh sách.
:::

Hoặc xoá cả các folder untracked:

```bash
git clean -fd
```

Trong đó:
- `-f`: force (bắt buộc)
- `-d`: bao gồm cả folder untracked

Để xóa file untracked nhưng giữ lại một số file cụ thể:

```bash
git clean -fd \
  -e config.local.json \
  -e notes.txt
```

Hoặc giữ một folder:

```bash
git clean -fd -e uploads/
```

## 14. Tag

Tag đánh dấu một commit cụ thể, thường dùng cho release (`v1.0.0`). Có 2 loại: **lightweight** (chỉ là con trỏ) và **annotated** (có tác giả, ngày, message — nên dùng cho release).

| Lệnh | Ý nghĩa |
|---|---|
| `git tag` | Liệt kê tag |
| `git tag -l "v1.*"` | Lọc tag theo pattern |
| `git tag v1.0.0` | Tạo lightweight tag tại HEAD |
| `git tag -a v1.0.0 -m "Release 1.0"` | Tạo annotated tag |
| `git tag -a v1.0.0 <commit>` | Tag một commit cũ |
| `git show v1.0.0` | Xem chi tiết tag |
| `git push origin v1.0.0` | Đẩy một tag lên remote |
| `git push --tags` | Đẩy tất cả tag |
| `git tag -d v1.0.0` | Xoá tag local |
| `git push origin --delete v1.0.0` | Xoá tag trên remote |
| `git checkout v1.0.0` | Checkout code tại tag (detached HEAD) |

## 15. Tìm kiếm và gỡ lỗi (grep, bisect, reflog)

### git grep — tìm trong code

| Lệnh | Ý nghĩa |
|---|---|
| `git grep "pattern"` | Tìm trong các file tracked |
| `git grep -n "pattern"` | Kèm số dòng |
| `git grep "pattern" <commit>` | Tìm trong một commit/tag cũ |

### git bisect — tìm commit gây bug

Tìm kiếm nhị phân qua lịch sử để xác định commit đầu tiên gây lỗi:

```bash
git bisect start
git bisect bad                # commit hiện tại bị lỗi
git bisect good v1.0.0        # bản v1.0.0 còn chạy tốt
# Git checkout commit ở giữa — ta test rồi trả lời:
git bisect good               # hoặc: git bisect bad
# ... lặp lại đến khi Git chỉ ra commit gây lỗi
git bisect reset              # quay về trạng thái ban đầu
```

Nếu có script test tự động (exit 0 = tốt): `git bisect run ./test.sh`

### git reflog — cứu hộ

`reflog` ghi lại mọi lần HEAD di chuyển (commit, reset, rebase, checkout...) trong local. Đây là phao cứu sinh khi lỡ tay `reset --hard` hay xoá branch:

```bash
git reflog                    # xem lịch sử di chuyển của HEAD
git reset --hard HEAD@{2}     # quay về vị trí 2 bước trước
git branch recover <hash>     # tạo branch giữ lại commit tưởng đã mất
```

:::tip Ghi chú
Reflog chỉ tồn tại trên máy local (mặc định giữ ~90 ngày), không được push lên remote.
:::

## 16. .gitignore

File `.gitignore` liệt kê các pattern mà Git bỏ qua (không track):

```gitignore
# Comment
*.o                 # mọi file .o
build/              # cả thư mục build
/config.json        # chỉ file ở gốc repo (có / đầu)
doc/**/*.pdf        # mọi file .pdf trong doc/ ở mọi cấp
!keep.o             # ngoại lệ — vẫn track file này
```

:::warning Lưu ý
`.gitignore` chỉ có tác dụng với file chưa tracked. Nếu file đã lỡ commit, phải gỡ khỏi index trước:

```bash
git rm --cached <file>      # gỡ khỏi Git nhưng giữ file trên đĩa
```
:::
