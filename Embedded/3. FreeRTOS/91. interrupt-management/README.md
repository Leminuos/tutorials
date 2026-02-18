Trong hệ điều hành RTOS, cần xử lý nhiều sự kiện khác nhau. Những sự kiện này thường được tạo ra bởi hardware interrupt. Vậy các ngắt này nên được xử lý như thế nào?

Giả sử hệ thống hiện đang chạy task 1, và người dùng nhấn một phím, gây ra ngắt. Quá trình ISR này như sau:
- CPU nhảy đến một địa chỉ cố định để thực thi mã; địa chỉ cố định này thường được gọi là vector ngắt. Việc nhảy này được thực hiện hoàn toàn bằng phần cứng.
- Việc thực thi mã lệnh có tác dụng gì?
  - Lưu execution context: Nếu task1 bị gián đoạn, cần phải lưu lại execution context của task1, chẳng hạn như giá trị của các thanh ghi core.
  - Xác định ngắt và gọi hàm ISR.
  - Khôi phục trạng thái hiện tại: Tiếp tục chạy task 1, hoặc chạy các task có ưu tiên cao hơn khác.

Lưu ý rằng hàm ISR được gọi bên trong kernel, và các task người dùng không thể được thực thi trong quá trình ISR đang chạy. Hàm ISR cần phải càng nhanh càng tốt; nếu không thì:
  - Các ngắt có mức độ ưu tiên thấp khác không thể được xử lý: tính realtime không thể được đảm bảo.
  - Các task người dùng không thể thực thi: Hệ thống hoạt động rất chậm.

Nếu xử lý hardware interrupt cần tốn rất nhiều thời gian thì sao? Việc xử lý ngắt này cần được chia thành hai phần:
- Hàm ISR: Nhanh chóng thực hiện một số công việc dọn dẹp và ghi chép, sau đó kích hoạt một task.
- Task: Xử lý các vấn đề phức tạp hơn.

Do đó cần có sự liên lạc giữa hàm ISR và task.

Để thành thạo việc sử dụng ngắt trong hệ điều hành RTOS, trước tiên cần nắm rõ một vài nguyên tắc:
- Hệ điều hành RTOS coi các task là độc lập với phần cứng; độ ưu tiên của task được xác định bởi developer, và bộ lập lịch sẽ quyết định khi nào một task được thực thi.
- Mặc dù hàm ISR cũng được triển khai bằng phần mềm, nhưng nó được coi là một phần của phần cứng vì nó có liên quan mật thiết đến phần cứng.
  - Hàm ISR sẽ được thực hiện khi nào? Tùy thuộc vào phần cứng.
  - Và hàm ISR nào được thực thi? Điều đó được quyết định bởi phần cứng.
- Các ngắt có độ ưu tiên cao hơn các task: ngay cả ngắt có độ ưu tiên thấp nhất cũng có độ ưu tiên cao hơn một task. Một task chỉ có thể được thực thi nếu không có ngắt nào xảy ra.

## Tại sao cần hai API khác nhau cho cùng một chức năng

Trong FreeRTOS, khi sử dụng queue, semaphore, software timer,... thì ta thấy có hai bộ API khác nhau được sử dụng, chẳng hạn như hàm gửi vào queue có hai API: `xQueueSendToBack` dùng cho task và `xQueueSendToBackFromISR` dùng cho hàm ISR.

Vậy tại sao lại có điều này?
- Nhiều hàm API có thể khiến các task rơi vào trạng thái blocked. Ví dụ, khi ghi dữ liệu vào queue, nếu queue đầy, nó có thể chuyển sang trạng thái blocked và phải chờ một khoảng thời gian.
- Khi một ISR gọi một hàm API, ISR đó không phải là một task và không thể rơi vào trạng thái blocked.
- Do đó, chức năng của các vai trò này khác nhau đối với task và đối với các hàm ISR.

Vậy tại sao không phân biệt người gọi hiện tại là một task hay một hàm ISR bên trong một hàm? Ví dụ code như sau:

```c
BaseType_t xQueueSend(...)
{
    if (is_in_isr())
    {

    }
    else
    {

    }
}
```

FreeRTOS sử dụng hai bộ API thay vì một bộ API duy nhất vì những ưu điểm sau:
- Việc sử dụng cùng một API đòi hỏi phải thêm mã điều kiện và các nhánh rẽ bổ sung, làm cho các hàm dài hơn, phức tạp hơn và khó debug hơn.
- Các tham số cần thiết sẽ khác nhau khi gọi hàm trong một task hoặc một hàm ISR, ví dụ:
  - Khi gọi hàm này trong một task, cần phải chỉ định một khoảng thời gian chờ, cho biết rằng task sẽ bị block trong một thời gian ngắn nếu nó thất bại.
  - Gọi hàm bên trong ISR: Không cần chỉ định thời gian chờ, hàm sẽ trả về ngay lập tức bất kể thành công hay thất bại.
  - Việc kết hợp hai nhóm API một cách cưỡng ép sẽ dẫn đến số lượng tham số quá lớn và kém hiệu quả.
- Khi chuyển đổi FreeRTOS, cần phải cung cấp thêm các chức năng để giám sát ngữ cảnh, chẳng hạn như...`is_in_isr()`
- Một số kiến ​​trúc microprocessor không thể dễ dàng phân biệt liệu chúng hiện đang thực thi một task hay đang ở trong một hàm ISR, điều này đòi hỏi phải bổ sung thêm nhiều đoạn mã phức tạp hơn.

## Tham số xHigherPriorityTaskWoken

Tham số `xHigherPriorityTaskWoken` được sử dụng để lưu kết quả của hàm: cho biết khi thực hiện hàm "FromISR" trong ISR có làm cho một task có độ ưu tiên cao hơn task đang chạy chuyển sang trạng thái ready hay không:
- `pdTRUE`: Thực hiện chuyển task
- `pdFALSE`: Không cần chuyển task.

:::tip
`xHigherPriorityTaskWoken` không dùng để wake task, mà dùng để báo cho kernel rằng một task ưu tiên cao hơn vừa chuyển sang trạng thái ready và cần được chuyển sang chạy ngay khi ISR kết thúc.
:::

:::warning Chú ý
Hàm ISR không tự chuyển task mà nó thông báo cho kernel biết có cần context switch hay không thông qua cờ `xHigherPriorityTaskWorker`.
:::

Chúng ta hãy lấy việc ghi vào queue làm ví dụ một lần nữa.

Khi task A gọi hàng `xQueueSendToBack()` để ghi, một số trường hợp có thể xảy ra:
 - Queue đã đầy, task A bị block và đang chờ, trong khi một task khác là B đang chạy.
 - Queue chưa đầy và task A đã ghi vào queue thành công. Tuy nhiên, điều này đã kích hoạt một task khác là task B. task B có độ ưu tiên cao hơn -> task B cần chạy trước.
 - Queue chưa đầy và task A đã ghi vào queue thành công và sẽ trả về ngay lập tức.

Hàm `xQueueSendToBackFromISR()` cũng có thể gây ra việc chuyển task, nhưng việc chuyển đổi sẽ không diễn ra bên trong hàm. Thay vào đó, chúng trả về một tham số cho biết liệu có cần chuyển đổi hay không.

Ví dụ code như sau:

```c
void XXX_ISR()
{
    int i;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    for (i = 0; i < N; i++)
    {
        xQueueSendToBackFromISR(..., &xHigherPriorityTaskWoken);
    }

    if (xHigherPriorityTaskWoken == pdTRUE)
    {
            
    }
}
```

Một hàm ISR có thể gọi hàm "FromISR" nhiều lần như sau:

```c
void XXX_ISR()
{
    int i;
    for (i = 0; i < N; i++)
    {
        xQueueSendToBackFromISR(...);
    }
}
```

Việc context switch bên trong "FromISR" là không hợp lý vì ngữ cảnh hiện tại đang là interrupt. Giải pháp là:
- Trong hàm "FromISR", hãy đánh dấu xem có cần chuyển đổi hay không.
- Thực hiện chuyển task trước khi hàm ISR trả về.

Các ví dụ trên rất phổ biến, chẳng hạn như ngắt UART: nhiều ký tự được đọc từ hàm ISR của UART, và chuyển task chỉ xảy ra khi nhận được ký tự xuống dòng.

Khi sử dụng hàm "FromISR", nếu không muốn sử dụng tham số `xHigherPriorityTaskWorker`, ta có thể đặt nó thành `NULL`. Tuy nhiên, nếu đặt thành `NULL` và không thực hiện chuyển task thì khi task có độ ưu tiên cao hơn chuyển sang trạng thái ready, nó sẽ không được chạy ngay mà chỉ được chạy ở tick tiếp theo hoặc task hiện tại block. 

## Làm thế nào để chuyển task

Trong FreeRTOS có hỗ trợ hai macro được sử dụng trong hàm ISR để chuyển task:

```c
portEND_SWITCHING_ISR( xHigherPriorityTaskWoken );

portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
```

Trong đó:
- `portEND_SWITCHING_ISR` được triển khai bằng mã assembly.
- `portYIELD_FROM_ISR` được triển khai bằng mã C.

Tuy nhiên, tất cả các version mới của FreeRTOS thì khuyến khích sử dụng macro `portYIELD_FROM_ISR`.

Ví dụ cách sử dụng như sau:

```c
void XXX_ISR()
{
    int i;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    for (i = 0; i < N; i++)
    {
        xQueueSendToBackFromISR(..., &xHigherPriorityTaskWoken);
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
```

## Tham khảo

https://rtos.100ask.net/zh/FreeRTOS/simulator/chapter11.html