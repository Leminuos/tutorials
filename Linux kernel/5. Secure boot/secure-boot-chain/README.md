# Secure Boot Chain

Secure boot chain trên AM335x (SoC của BBB) hoạt động theo chuỗi stage: ROM -> MLO (SPL) -> U-Boot -> Kernel -> Rootfs, trong đó mỗi stage verify stage tiếp theo trước khi chuyển quyền thực thi.

Tuy nhiên, có một điểm quan trọng cần lưu ý: AM335x trên BeagleBone Black thương mại sử dụng General Purpose (GP) device, nghĩa là ROM bootloader không hỗ trợ secure boot hoàn chỉnh như dòng High Security (High Security).

Ta cần kiểm tra SoC trên board:

```bash
# Chạy trên BBB
cat /sys/bus/soc/devices/soc0/type
```

Kết quả sẽ cho biết device type: GP hay HS. Topic này sẽ trình bày cả hai hướng để ta hiểu rõ secure boot chain của từng loại.