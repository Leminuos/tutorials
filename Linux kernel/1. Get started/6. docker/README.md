## 1. Docker là gì?

Docker là một phần mềm mã nguồn mở, ra đời năm 2013, thuộc loại containerization platform. Nói đơn giản, Docker cho phép ta tạo ra những môi trường cô lập được gọi là container trên cùng một máy tính, mỗi container hoạt động độc lập như thể nó là một máy tính riêng.

Ví dụ cụ thể: ta đang phát triển một website dùng Python 3.11 + PostgreSQL 15. Thay vì cài Python và PostgreSQL trực tiếp lên máy, ta tạo container chứa sẵn Python 3.11 và container khác chứa PostgreSQL 15. Hai container này chạy song song, không ảnh hưởng gì đến hệ thống máy. Khi không cần nữa, xóa container sẽ không để lại rác trên máy.

## 2. Tại sao cần Docker?

Có nhiều lý do thực tế:
- Nhất quán môi trường: Đây là lý do lớn nhất. Khi ta làm việc nhóm, mỗi người có máy khác nhau — người dùng Windows, người dùng Mac, người dùng Linux. Phiên bản phần mềm cũng khác nhau. Docker giải quyết triệt để: mọi người cùng chạy một container giống hệt nhau, không còn chuyện "máy tôi chạy được mà máy bạn không chạy."
- Cô lập ứng dụng: Giả sử ta có 2 dự án: dự án A cần Node.js 16, dự án B cần Node.js 20. Cài cả hai phiên bản trên cùng máy rất dễ xung đột. Với Docker, mỗi dự án chạy trong container riêng, mỗi container có phiên bản Node.js riêng, hoàn toàn không ảnh hưởng lẫn nhau.
- Triển khai dễ dàng: Khi ứng dụng chạy tốt trong container trên máy, ta chỉ cần đưa nguyên container đó lên server. Không cần lo cấu hình server, cài đặt thư viện — container đã chứa sẵn mọi thứ.
- Khởi động nhanh, nhẹ tài nguyên: Một container khởi động trong 1–2 giây, trong khi máy ảo cần vài phút. Container cũng chỉ tốn vài chục MB RAM thay vì hàng GB như máy ảo.
- Dễ dọn dẹp: Muốn thử một công nghệ mới ví dụ Redis, MongoDB, Elasticsearch,...? Chỉ cần chạy container của nó, dùng thử, rồi xóa. Máy ta hoàn toàn sạch, không bị xả rá" bởi những thứ cài rồi gỡ không hết.

## 3. Các khái niệm cốt lõi

### 3.1. Image

Image là bản thiết kế read only chứa mọi thứ cần thiết để chạy ứng dụng: background OS thường là bản Linux thu gọn, library, source, environment variable và lệnh khởi chạy.

Đặc điểm quan trọng của image là cấu trúc layer. Mỗi dòng lệnh trong Dockerfile tạo ra một layer. Ví dụ:
- Layer 1: Hệ điều hành Ubuntu
- Layer 2: Cài Python
- Layer 3: Cài thư viện (pip install)
- Layer 4: Copy mã nguồn vào

Lợi ích là các image khác nhau có thể chia sẻ các layer chung. Nếu ta có 5 ứng dụng Python, cả 5 đều dùng chung layer Ubuntu và layer Python, tiết kiệm rất nhiều dung lượng ổ cứng.

Các lệnh docker liên quan đến image:

**Liệt kê image đang có trên máy**

```bash
docker images                  # liệt kê tất cả
```

**Build image từ Dockerfile**

```bash
docker build -t my-app .
docker build -t my-app:1.0 .            # build với tag phiên bản
```

Giải thích:
- `-t my-app:1.0`: đặt tên kèm tag cho image, với `my-app` là tên và `1.0` là tag phiên bản
- `.`: build context, tức thư mục chứa Dockerfile và source code

Ta cũng có thể build image với các option sau:

```bash
docker build -f Dockerfile.dev -t my-app-dev .  # chỉ định file Dockerfile khác
docker build --no-cache -t my-app .     # build lại từ đầu, không dùng cache
```

**Xoá image**

```bash
docker rmi nginx               # xóa image nginx
docker rmi nginx:1.25          # xóa phiên bản cụ thể
```

Ta cũng có thể xoá tất cả image trong máy bằng lệnh sau:

```bash
docker rmi $(docker images -q) # xóa tất cả image
```

**Xem các layer của image**

```bash
docker history nginx           # xem từng layer được tạo như thế nào
```

### 3.2. Container

Container là một instance đang chạy của image. Mối quan hệ giữa image và container giống như mối quan hệ giữa class và object trong lập trình trong đó image là class, container là object.

Mỗi container có:
- Hệ thống file riêng (cô lập với máy chủ)
- Mạng riêng (có IP riêng)
- Tiến trình riêng (không thấy tiến trình của container khác)

Nhưng tất cả container chia sẻ chung kernel của hệ điều hành chủ, đây chính là lý do container nhẹ hơn máy ảo rất nhiều.

Ví dụ hello world kinh điển:

```bash
docker run hello-world
```

Khi chạy lệnh này, Docker sẽ làm những việc sau:
- Tìm image `hello-world` trên máy -> không thấy
- Tự động tải image từ Docker Hub
- Tạo container từ image đó
- Chạy container -> in ra thông báo chào mừng
- Container tự tắt sau khi hoàn thành

Nếu ta thấy dòng "Hello from Docker!" nghĩa là mọi thứ hoạt động đúng.

Ví dụ chạy container ubuntu:

```bash
docker run -it ubuntu bash
```

Giải thích:
- `docker run`: tạo và chạy container
- `-it`: chế độ tương tác (interactive + terminal), cho phép ta gõ lệnh
- `ubuntu`: image Ubuntu
- `bash`: chạy shell bash khi container khởi động

Ta sẽ thấy prompt đổi thành kiểu `root@a1b2c3d4e5f6:/#`, lúc này ta đang bên trong container Ubuntu. Thử gõ vài lệnh:

```bash
cat /etc/os-release    # xem thông tin OS
ls /                   # xem cấu trúc thư mục
apt update             # cập nhật danh sách gói
exit                   # thoát khỏi container
```

Ví dụ chạy một webserver Nginx:

```bash
docker run -d -p 8080:80 --name my-web nginx
```

Giải thích:
- `-d`: chạy nền (detached), không chiếm terminal
- `-p 8080:80`: ánh xạ cổng 8080 trên máy vào cổng 80 trong container
- `--name my-web`: đặt tên container cho dễ quản lý
- `nginx`: image web server Nginx

Sau khi chạy, mở trình duyệt vào `http://localhost:8080` sẽ thấy trang chào mừng của Nginx.

Các lệnh docker liên quan đến container:

**Tạo và chạy container**

```bash
# Cơ bản
docker run nginx                        # chạy đơn giản
docker run -d nginx                     # chạy nền (detached)
docker run -it ubuntu bash              # chạy tương tác với terminal

# Đặt tên
docker run -d --name my-web nginx       # đặt tên container

# Ánh xạ cổng
docker run -d -p 8080:80 nginx                    # mapping port máy:8080 -> container:80
docker run -d -p 3000:3000 -p 3001:3001 node-app  # nhiều cổng

# Mount volume
docker run -d -v /home/user/data:/app/data nginx           # bind mount
docker run -d -v my-volume:/app/data nginx                 # named volume
docker run -d -v $(pwd):/app node-app                      # mount thư mục hiện tại

# Biến môi trường
docker run -d -e MYSQL_ROOT_PASSWORD=secret mysql          # một biến
docker run -d --env-file .env my-app                       # đọc từ file .env

# Giới hạn tài nguyên
docker run -d --memory=512m --cpus=1.5 my-app              # giới hạn RAM và CPU

# Tự xóa container khi dừng
docker run --rm ubuntu
```

**Liệt kệ các container**

```bash
docker ps                      # hiển thị các container đang chạy
docker ps -a                   # hiển thị tất cả kể cả container đã dừng
docker ps -q                   # chỉ hiện ID (dùng để kết hợp lệnh khác)
```

**Điều khiển container**

```bash
docker stop my-web
docker start my-web
docker restart my-web
docker stop $(docker ps -q)    # dừng tất cả container đang chạy
```

**Chạy lệnh bên trong container đang chạy**

```bash
docker exec -it my-web bash              # mở shell trong container
docker exec -it my-web sh                # dùng sh nếu container không có bash
docker exec my-web cat /etc/nginx/nginx.conf  # xem file cấu hình
docker exec -it my-db mysql -u root -p   # mở MySQL client trong container
```

**Xem log của container**

```bash
docker logs my-web                 # toàn bộ log
docker logs -f my-web              # theo dõi log realtime (như tail -f)
docker logs --tail 50 my-web       # 50 dòng cuối
docker logs --since 1h my-web      # log trong 1 giờ gần nhất
```

**Xóa container**

```bash
docker rm my-web                   # trước khi xoá phải dừng trước
docker rm -f my-web                # dừng + xóa luôn
docker rm $(docker ps -aq)         # xóa tất cả container
```

### 3.3. Dockerfile

Đây là file văn bản thuần không có đuôi mở rộng, tên mặc định là `Dockerfile`. File này chứa các instruction từng bước để Docker build ra một image.

#### 3.3.1. Các chỉ thị quan trọng

**Lệnh FORM:**

```dockerfile
FROM python:3.11-slim
```

Mọi `Dockerfile` đều bắt đầu bằng `FROM`. Ta không build từ con số 0 mà luôn xây dựng trên một image có sẵn. Một số image nền phổ biến:
- `python:3.11`: đầy đủ, nặng (~900MB)
- `python:3.11-slim`: bản rút gọn, nhẹ hơn (~150MB)
- `python:3.11-alpine`: siêu nhẹ (~50MB), nhưng đôi khi thiếu thư viện
- `node:20`, `openjdk:17`, `golang:1.22`: tương tự cho các ngôn ngữ khác
- `ubuntu:22.04`, `debian:bookworm`: khi ta cần hệ điều hành nền chung

**Lệnh WORKDIR:**

```dockerfile
WORKDIR /app
```

Tất cả lệnh sau đó (`RUN`, `COPY`, `CMD`...) sẽ thực thi trong thư mục này. Nếu thư mục chưa tồn tại, Docker tự tạo. Nên luôn dùng `WORKDIR` thay vì `RUN cd /app`, vì `cd` chỉ có tác dụng trong một lệnh `RUN`, không ảnh hưởng đến các lệnh tiếp theo.

**Lệnh COPY:**

```dockerfile
COPY . .                        # copy toàn bộ thư mục hiện tại vào WORKDIR
COPY requirements.txt .         # copy một file cụ thể
COPY src/ /app/src/             # copy thư mục src vào /app/src trong image
```

Lệnh `COPY` chỉ copy file. `ADD` có thêm khả năng tự giải nén `.tar.gz` và tải file từ URL, nhưng thực tế ta nên dùng `COPY` trừ khi cần giải nén vì `COPY` rõ ràng và dễ đoán hơn.

**Lệnh RUN:**

```dockerfile
RUN apt update && apt install -y curl
RUN pip install -r requirements.txt
RUN npm install
```

`RUN` thực thi lệnh và kết quả được lưu thành một layer mới trong image. Lưu ý quan trọng: mỗi `RUN` tạo một layer, nên hãy gộp các lệnh liên quan bằng `&&` để giảm số layer:

```dockerfile
# Không tốt: 3 layer riêng biệt
RUN apt update
RUN apt install -y curl
RUN apt clean

# Tốt: 1 layer duy nhất
RUN apt update && apt install -y curl && apt clean
```

**Lệnh CMD:**

```dockerfile
CMD ["python", "app.py"]
CMD ["node", "server.js"]
CMD ["nginx", "-g", "daemon off;"]
```

Mỗi `Dockerfile` chỉ có một `CMD` duy nhất. Nếu viết nhiều, chỉ cái cuối cùng có hiệu lực. `CMD` có thể bị ghi đè khi ta chạy `docker run <image> <lệnh_khác>`.

**Lệnh ENTRYPOINT:**

```dockerfile
ENTRYPOINT ["python"]
CMD ["app.py"]
```

Kết hợp này có nghĩa là mặc định chạy `python app.py`, nhưng người dùng có thể đổi thành `python test.py` bằng cách gõ `docker run <image> test.py`. `ENTRYPOINT` cố định phần `python`, `CMD` cung cấp tham số mặc định.

#### 3.3.2. Ví dụ thực tế

Ta có một ứng dụng python với cấu trúc thư mục như sau:

```
my-python-app/
├── app.py
├── requirements.txt
└── Dockerfile
```

File `Dockerfile` có nội dung như sau:

```dockerfile
# Chọn image nền nhẹ
FROM python:3.11-slim

# Đặt thư mục làm việc
WORKDIR /app

# Copy file requirements trước (để tận dụng cache — giải thích bên dưới)
COPY requirements.txt .

# Cài thư viện
RUN pip install -r requirements.txt

# Sau đó mới copy mã nguồn
COPY . .

# Khai báo cổng
EXPOSE 5000

# Lệnh chạy khi container khởi động
CMD ["python", "app.py"]
```

Mỗi dòng lệnh tạo một layer trong image. Nếu ta chỉ thay đổi mã nguồn mà không thay đổi thư viện, Docker sẽ dùng lại cache từ các layer cũ và chỉ build lại layer bị thay đổi, giúp tốc độ build nhanh hơn nhiều.

#### 3.3.3. File .dockerignore

Giống `.gitignore`, file này nói cho Docker biết không copy những gì vào image. Tạo file `.dockerignore `cùng thư mục với `Dockerfile`:

```
node_modules
__pycache__
.git
.env
*.log
.vscode
.idea
```

**Tại sao cần?** Vì khi chạy `COPY . .`, Docker copy tất cả file trong thư mục. Nếu không ignore, ta có thể vô tình đưa vào image cả thư mục `node_modules` nặng hàng trăm MB, file `.env` chứa mật khẩu hay thư mục `.git`.

#### 3.3.4. Cơ chế cache khi build

Docker cache từng layer theo thứ tự từ trên xuống. Nếu một layer không thay đổi, Docker dùng lại cache. Nhưng khi một layer thay đổi, tất cả layer phía sau đều phải build lại.

Vì vậy thứ tự trong Dockerfile rất quan trọng:

```dockerfile
# ❌ Không tốt
COPY . .                              # mã nguồn thay đổi thường xuyên
RUN pip install -r requirements.txt   # phải cài lại thư viện mỗi lần code đổi

# ✅ Tốt
COPY requirements.txt .              # thư viện ít khi đổi
RUN pip install -r requirements.txt  # layer này được cache
COPY . .                             # chỉ layer này build lại khi code đổi
```

Nguyên tắc chung: đặt những thứ ít thay đổi ở trên, thứ hay thay đổi ở dưới.

#### 3.3.5. Multi-stage build

Multi-stage build cho phép ta dùng nhiều `FROM` trong cùng một `Dockerfile`. Mục đích là tách biệt giai đoạn build cần nhiều công cụ, nặng và giai đoạn chạy chỉ cần kết quả, nhẹ.

```dockerfile
# === Giai đoạn 1: Build ===
FROM node:20 AS builder
WORKDIR /app
COPY package*.json ./
RUN npm ci
COPY . .
RUN npm run build          # tạo thư mục dist/

# === Giai đoạn 2: Chạy ===
FROM nginx:alpine
COPY --from=builder /app/dist /usr/share/nginx/html
EXPOSE 80
CMD ["nginx", "-g", "daemon off;"]
```

Image cuối cùng chỉ chứa Nginx + file HTML/JS/CSS đã build. Không còn Node.js, không còn `node_modules`, không còn mã nguồn gốc. Kết quả: image từ ~1GB giảm xuống còn ~30MB.

### 3.4. Docker Hub

Docker Hub hay Registry là kho chứa image, giống như GitHub là kho chứa mã nguồn. Docker Hub là registry công khai lớn nhất, chứa hàng triệu image có sẵn. Ta có thể tải về image chính thức của hầu hết mọi phần mềm phổ biến: Ubuntu, MySQL, Redis, Node.js, Nginx... chỉ bằng một lệnh docker pull.

Ngoài Docker Hub, ta cũng có thể dùng registry riêng (private registry) của công ty hoặc các dịch vụ như AWS ECR, Google GCR, GitHub Container Registry.

**Tải image từ Docker Hub về máy**

```bash
docker pull nginx              # tải bản latest (mặc định)
docker pull nginx:1.25         # tải phiên bản cụ thể
docker pull python:3.11-slim   # bản Python nhẹ (slim)
```

**Gắn tag cho image trước khi push lên Docker hub**

```bash
docker tag my-app myname/my-app:1.0
```

**Đẩy image lên registry**

```bash
docker login                   # đăng nhập Docker Hub trước
docker push myname/my-app:1.0  # push image lên
```

### 3.5. Volume

Mặc định, khi container bị xóa, toàn bộ dữ liệu bên trong cũng mất theo. Volume giải quyết vấn đề này, nó là cơ chế để lưu trữ dữ liệu persistent bên ngoài container. Ví dụ, database chạy trong container nhưng dữ liệu được lưu trên volume, nên dù xóa container và tạo lại, dữ liệu vẫn còn.

Các lệnh docker liên quan đến volume:

```bash
docker volume create my-data       # tạo volume
docker volume ls                   # liệt kê volume
docker volume inspect my-data      # xem chi tiết
docker volume rm my-data           # xóa volume
docker volume prune                # xóa tất cả volume không dùng
```

### 3.6. Network

Docker tạo mạng ảo để các container giao tiếp với nhau. Ví dụ, container chạy ứng dụng web có thể kết nối đến container chạy database thông qua mạng nội bộ của Docker, mà không cần mở cổng ra ngoài.

Các lệnh docker liên quan đến network:

```bash
docker network create my-net       # tạo network
docker network ls                  # liệt kê network
docker network inspect my-net      # xem chi tiết
docker network rm my-net           # xóa network
```

## 4. Docker compose

### 4.1. Vấn đề mà Docker Compose giải quyết

Trong thực tế, một ứng dụng hiếm khi chạy đơn lẻ. Một web app điển hình có thể cần: web server (Nginx), backend (Python/Node), database (PostgreSQL), cache (Redis), queue (RabbitMQ)... Mỗi thứ chạy trong một container riêng.

Không có Compose, ta phải gõ từng lệnh `docker run` dài dòng cho mỗi container:

```bash
docker network create my-network

docker run -d --name db --network my-network \
  -e POSTGRES_PASSWORD=secret \
  -v db-data:/var/lib/postgresql/data \
  postgres:16

docker run -d --name redis --network my-network \
  redis:7-alpine

docker run -d --name web --network my-network \
  -p 8000:8000 \
  -e DATABASE_URL=postgresql://postgres:secret@db:5432/mydb \
  my-web-app
```

Ba container thôi đã dài như vậy. Tưởng tượng 10 container, mỗi lần khởi động phải gõ lại, rất dễ sai và mệt mỏi. Docker Compose giải quyết bằng cách gom tất cả vào một file cấu hình duy nhất và chỉ cần chạy một lệnh là toàn bộ hệ thống hoạt động.

### 4.2. File docker-compose.yml

File được viết bằng cú pháp YAML. Đặt ở thư mục root dự án. Cấu trúc tổng quát:

```yaml
services:
  service_1:
    # cấu hình container thứ nhất
  service_2:
    # cấu hình container thứ hai

volumes:
  # khai báo volume dùng chung

networks:
  # khai báo network (tùy chọn)
```

Mỗi service tương ứng với một container. Tên service đồng thời là hostname trong mạng nội bộ, các container dùng tên này để gọi nhau.

**Ví dụ: Web app + Database**

Đây là cấu hình phổ biến nhất cho người mới bắt đầu:

```yaml
services:
  web:
    build: .                          # build image từ Dockerfile trong thư mục hiện tại
    ports:
      - "8000:8000"                   # ánh xạ cổng
    environment:
      - DATABASE_URL=postgresql://postgres:secret@db:5432/mydb
    depends_on:
      - db                            # đảm bảo db khởi động trước web

  db:
    image: postgres:16                # dùng image có sẵn từ Docker Hub
    environment:
      - POSTGRES_PASSWORD=secret
      - POSTGRES_DB=mydb
    volumes:
      - db-data:/var/lib/postgresql/data    # lưu dữ liệu bền vững

volumes:
  db-data:                            # khai báo volume
```

### 4.3. Giải thích chi tiết từng thuộc tính

**`build` vs `image`**

```yaml
# Cách 1: dùng image có sẵn từ Docker Hub
db:
  image: postgres:16

# Cách 2: build từ Dockerfile trong thư mục hiện tại
web:
  build: .

# Cách 3: build với tùy chọn chi tiết
web:
  build:
    context: ./backend              # thư mục chứa code và Dockerfile
    dockerfile: Dockerfile.prod     # dùng Dockerfile khác tên mặc định
    args:
      - PYTHON_VERSION=3.12        # truyền ARG vào Dockerfile
```

**`ports` vs `expose`**

```yaml
web:
  ports:
    - "8080:8000"      # máy_chủ:container — truy cập được từ bên ngoài
    - "443:443"

backend:
  expose:
    - "8000"           # chỉ mở trong mạng nội bộ Docker, bên ngoài không thấy
```

Dùng `ports` cho service cần truy cập từ trình duyệt (Nginx, frontend). Dùng `expose` cho service chỉ giao tiếp nội bộ (backend, database).

**Lưu trữ dữ liệu**

```yaml
services:
  db:
    volumes:
      # Named volume — Docker quản lý, dữ liệu tồn tại khi xóa container
      - db-data:/var/lib/postgresql/data

      # Bind mount — gắn thư mục từ máy chủ vào container
      - ./init.sql:/docker-entrypoint-initdb.d/init.sql

      # Bind mount read-only
      - ./config:/app/config:ro

volumes:
  db-data:        # khai báo named volume ở đây
```

Hai loại volume chính:
- named volume: Docker quản lý, dùng cho dữ liệu database, cache
- bind mount: gắn thư mục trên máy vào container, rất hữu ích khi phát triển. Thay đổi code trên máy sẽ phản ánh ngay trong container.

**Thứ tự khởi động**

```yaml
# Cách đơn giản: chỉ đảm bảo container kia được tạo trước
backend:
  depends_on:
    - db
    - redis

# Cách nâng cao: chờ service thật sự sẵn sàng
backend:
  depends_on:
    db:
      condition: service_healthy     # chờ healthcheck pass
    redis:
      condition: service_started     # chỉ cần container đã bắt đầu
```

:::warning Lưu ý quan trọng
`depends_on` đơn giản chỉ đảm bảo container được tạo trước, không đảm bảo service bên trong sẵn sàng. PostgreSQL có thể mất vài giây sau khi container khởi động mới chấp nhận kết nối. Dùng `healthcheck` + `condition`: `service_healthy` để giải quyết.
:::

**Biến môi trường**

```yaml
# Cách 1: liệt kê trực tiếp
backend:
  environment:
    - DATABASE_URL=postgresql://postgres:secret@db:5432/app
    - DEBUG=true

# Cách 2: dùng file .env riêng
backend:
  env_file:
    - .env              # Docker Compose tự đọc file này
```

File `.env` ở cùng thư mục:

```
DATABASE_URL=postgresql://postgres:secret@db:5432/app
SECRET_KEY=super-secret-key
DEBUG=false
```

Cách 2 tốt hơn vì ta có thể thêm `.env` vào `.gitignore`, tránh commit mật khẩu lên Git.

**Khởi động lại**

```yaml
backend:
  restart: unless-stopped
```

Các giá trị:
- `no`: không tự khởi động lại (mặc định)
- `always`: luôn khởi động lại khi container dừng
- `on-failure`: chỉ khởi động lại khi exit code khác 0
- `unless-stopped`: như `always` nhưng không khởi động lại nếu ta chủ động d`ocker stop`

**Kiểm tra sức khỏe**

```yaml
db:
  healthcheck:
    test: ["CMD-SHELL", "pg_isready -U postgres"]
    interval: 10s       # kiểm tra mỗi 10 giây
    timeout: 5s         # timeout mỗi lần kiểm tra
    retries: 5          # thử lại 5 lần trước khi đánh dấu unhealthy
    start_period: 30s   # chờ 30 giây sau khi container khởi động mới bắt đầu kiểm tra
```

### 4.4. Các lệnh Docker compose

```bash
# Khởi động toàn bộ hệ thống
docker compose up                # chạy foreground (thấy log)
docker compose up -d             # chạy nền (detached)

# Dừng và xóa
docker compose down              # dừng container + xóa network
docker compose down -v           # dừng + xóa luôn volume (mất dữ liệu!)

# Build lại image
docker compose build             # build tất cả service có "build:"
docker compose up -d --build     # build lại rồi chạy luôn

# Xem trạng thái
docker compose ps                # danh sách container
docker compose logs              # xem log tất cả service
docker compose logs -f backend   # theo dõi log realtime của một service
docker compose top               # xem tiến trình đang chạy

# Thao tác từng service
docker compose start backend     # chạy một service
docker compose stop backend      # dừng một service
docker compose restart backend   # khởi động lại
docker compose exec backend bash # vào shell của container đang chạy
```

## 5. Cài đặt docker trên ubuntu

2 cách cài: Script tự động và cài thủ công.

### 5.1. Script tự động

```bash
curl -fsSL https://get.docker.com -o get-docker.sh
sudo sh get-docker.sh
```

Chỉ 2 dòng là xong. Script này tự phát hiện phiên bản Ubuntu và cài đặt Docker phù hợp.

### 5.2. Cài thủ công

**Bước 1: Gỡ bản Docker cũ (nếu có)**

```bash
sudo apt remove docker docker-engine docker.io containerd runc
```

Lệnh này đảm bảo không còn bản Docker cũ gây xung đột.

**Bước 2: Cài các gói phụ thuộc**

```bash
sudo apt update
sudo apt install ca-certificates curl gnupg
```

Đây là các công cụ cần thiết để tải và xác thực gói cài đặt.

**Bước 3: Thêm GPG key của Docker**

```bash
sudo install -m 0755 -d /etc/apt/keyrings
curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo gpg --dearmor -o /etc/apt/keyrings/docker.gpg
sudo chmod a+r /etc/apt/keyrings/docker.gpg
```

GPG key dùng để xác minh gói cài đặt thật sự đến từ Docker, không bị giả mạo.

**Bước 4: Thêm repository của Docker**

```bash
echo \
  "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.gpg] https://download.docker.com/linux/ubuntu \
  $(. /etc/os-release && echo "$VERSION_CODENAME") stable" | \
  sudo tee /etc/apt/sources.list.d/docker.list > /dev/null
```

Lệnh này thêm repository chính thức của Docker vào hệ thống.

**Bước 5: Cài Docker**

```bash
sudo apt update
sudo apt install docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin
```

Giải thích từng gói:
- `docker-ce`: Docker Engine
- `docker-ce-cli`: công cụ dòng lệnh
- `containerd.io`: runtime quản lý container
- `docker-buildx-plugin`: plugin build image nâng cao
- `docker-compose-plugin`: chạy nhiều container cùng lúc

### 5.3. Sau khi cài thành công

Cho phép chạy Docker không cần `sudo`:

```bash
sudo usermod -aG docker $USER
newgrp docker
```

Mặc định, Docker yêu cầu quyền root. Lệnh trên thêm user vào nhóm docker để chạy lệnh mà không cần gõ `sudo` mỗi lần.

**Kiểm tra cài đặt thành công**

```bash
docker --version
```

Kết quả sẽ hiện kiểu: `Docker version 27.x.x, build xxxxxxx`

