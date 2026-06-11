## Conventional Commits

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