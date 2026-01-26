## Mở đầu

RTOS - Real-Time Operating system hay hệ điều hành thời gian thực, sử dụng trong những ứng dụng yêu cầu về thời gian đáp ứng nhanh với độ chính xác cao về thời gian.

Ví dụ người dùng muốn hệ thống thực hiện nếu đo được khí gas bị rò thì sẽ gửi cảnh báo sau không quá 0,5s, thì việc hệ thống đáp ứng sau 0,1s hay 0,4s đều là real-time cả.

![RTOS Introduce](img/rtos-introduce.png)

Khi ngay cả nếu người dùng muốn cảnh báo sau không quá 10s, thì việc hệ thống đáp ứng sau 9s, mặc dù con số này khá lớn thì vẫn là đáp ứng thời gian thực real-time.

Một số RTOS phổ biến:
- FreeRTOS
- TinyOS
- ThreadX
- RTX
- ...

## Phân loại RTOS

RTOS có thể được chia ra làm 2 loại dựa vào độ trễ:

- Hard Real-time: Hệ thống phải thực hiện task trong khoảng thời gian quy định một cách chính xác, việc không tuân thủ thời gian quy định có thể dẫn đến hậu quả nghiêm trọng.
  Ví dụ: Túi khí trên ô tô cần bật lên trong khoảng thời gian nhỏ hơn 0,1s từ khi cảm biến phát hiện xe xảy ra va chạm nguy hiểm. Thì việc hệ thống không đáp ứng thời gian này có thể gây tai nạn nghiêm trọng cho người trên xe. Do đó, task đọc cảm biến và điều khiển túi khí phải yêu cầu là Hard Real-time.

- Soft Real-Time: Có thể không cần đáp ứng gắt gao về mặt thời gian như Hard Real-time, việc đôi khi bị trễ thời gian với các task này có thể không gây ra hậu quả nghiêm trọng.
  Chẳng hạn, việc bấm nút thì đèn sáng sau không quá 0,5s. Thì việc bị trễ 1 chút cũng sẽ không ảnh hưởng quá nhiều.

## Tham khảo

https://www.laptrinhdientu.com/2021/09/Core10.html
