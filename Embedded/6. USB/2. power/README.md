Một đơn vị power được gọi là unit load và được xác định là 100mA.

Phân loại device theo power:
- Low power Bus-powered device: Các device này lấy nguồn VBUS từ port USB và không thể yêu cầu power nhiều hơn 1 unit load.
- High power Bus-powered device: Các device này lấy nguồn VBUS từ port USB và không thể yêu cầu power nhiều hơn 1 unit load trước khi được config. Sau khi config, nó chỉ có thể yêu cầu power tối đa là 1 unit load.
- Self-powered device: Các device này dùng nguồn riêng và chỉ yêu cầu tối đa 1 unit load.

Power negotiation: Ban đầu, khi device được attach thì mặc định nó chỉ có thể tiêu thụ tối đa là 1 unit load cho đến khi host gửi SetConfiguration trong quá trình enumeration để đàm phán khả năng tiêu thụ dòng của device. Trong quá trình đàm phán, device có thể yêu cầu nhiều dòng hơn nhưng không được vượt 5 unit load. Khi bus không hoạt động → device tự chuyển sang trạng thái suspend. Khi ở trạng thái suspend thì device phải giảm mức tiêu thụ dưới 2.5mA.

Theo USB protocol, nếu data line ở trạng thái IDLE trong hơn 3ms thì đó được coi là suspend signal. Qua đó, device được yêu cầu phải chuyển sang trạng thái suspend trong vòng 10ms và giảm dòng điện cấp device xuống giá trị được chỉ định, đối với low power device thì là 500 µA và đối với high power device hoặc remote wakeup device thì là 2,5mA.

Ở điều kiện hoạt động bình thường, hub hoặc host sẽ định kỳ gửi các Start Of Frame packet sau mỗi 1ms đối với USB full speed và sau mỗi 125µs đối với USB high speed để ngăn device chuyển sang trạng thái suspend. Khi ở trạng thái suspend, device phải tiếp tục cung cấp nguồn cho điện trở pullup D+ hoặc D- để duy trì trạng thái IDLE.

Khi device ở trạng thái suspend, clock của device có thể dừng lại để chuyển sang chế độ low power mode. Để thoát khỏi trạng thái suspend, device có thể được wake thông qua host hoặc tự wake thông qua một external interrupt, qua đó device cũng có thể wake host.