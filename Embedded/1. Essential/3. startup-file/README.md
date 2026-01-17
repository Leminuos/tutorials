# Startup file

Startup file được thực hiện trước hàm main, là file khởi tạo môi trường ban đầu trước khi call hàm main và chứa vector table.

::: tip
Mỗi vi điều khiển sẽ có một vector table khác nhau.
:::

- Tạo một bảng vector table cho vi điều khiển tương ứng

    ```c
    uint32_t vectors[] __attribute__((section(".isr_vector"))) = {
        STACK_START,
        (uint32_t)Reset_Handler,
        (uint32_t)NMI_Handler,
        (uint32_t)HardFault_Handler,
        (uint32_t)MemManage_Handler,
        (uint32_t)BusFault_Handler,
        (uint32_t)UsageFault_Handler,
        0,
        0,
        0,
        0,
        (uint32_t)SVC_Handler,
        (uint32_t)DebugMon_Handler,
        0,
        (uint32_t)PendSV_Handler,
        (uint32_t)SysTick_Handler,
        (uint32_t)WWDG_IRQHandler,
        (uint32_t)PVD_IRQHandler,         
        (uint32_t)TAMP_STAMP_IRQHandler,  
        (uint32_t)RTC_WKUP_IRQHandler,    
        0,                      
        (uint32_t)RCC_IRQHandler,         
        (uint32_t)EXTI0_IRQHandler,       
        (uint32_t)EXTI1_IRQHandler,       
        (uint32_t)EXTI2_IRQHandler,       
        (uint32_t)EXTI3_IRQHandler,       
        (uint32_t)EXTI4_IRQHandler,          
        ...
        ...
        ...
        (uint32_t)OTG_HS_EP1_OUT_IRQHandler,
        (uint32_t)OTG_HS_EP1_IN_IRQHandler,
        (uint32_t)OTG_HS_WKUP_IRQHandler, 
        (uint32_t)OTG_HS_IRQHandler,      
        (uint32_t)DCMI_IRQHandler,        
        (uint32_t)CRYP_IRQHandler,        
        (uint32_t)HASH_RNG_IRQHandler,    
        (uint32_t)FPU_IRQHandler,      
    };
    ```

- Các handler trong vector table được đặt trong section `.isr_section` được quy định trong file linker script.

    ```c
    void Reset_Handler(void);

    void NMI_Handler 			(void) __attribute__ ((weak, alias("Default_Handler")));
    void HardFault_Handler 		(void) __attribute__ ((weak, alias("Default_Handler")));
    void MemManage_Handler 		(void) __attribute__ ((weak, alias("Default_Handler")));
    void BusFault_Handler 		(void) __attribute__ ((weak, alias("Default_Handler")));
    void UsageFault_Handler 	(void) __attribute__ ((weak, alias("Default_Handler")));
    void SVC_Handler 			(void) __attribute__ ((weak, alias("Default_Handler")));
    void DebugMon_Handler 		(void) __attribute__ ((weak, alias("Default_Handler")));
    void PendSV_Handler   		(void) __attribute__ ((weak, alias("Default_Handler")));
    void SysTick_Handler  		(void) __attribute__ ((weak, alias("Default_Handler")));
    void WWDG_IRQHandler 		(void) __attribute__ ((weak, alias("Default_Handler")));
    void PVD_IRQHandler 		(void) __attribute__ ((weak, alias("Default_Handler")));             
    void TAMP_STAMP_IRQHandler 	(void) __attribute__ ((weak, alias("Default_Handler")));      
    void RTC_WKUP_IRQHandler 	(void) __attribute__ ((weak, alias("Default_Handler")));                               
    void RCC_IRQHandler 		(void) __attribute__ ((weak, alias("Default_Handler")));             
    void EXTI0_IRQHandler 		(void) __attribute__ ((weak, alias("Default_Handler"))); 
    ...
    ...
    ...
    ```

- Định nghĩa cho hàm `Reset_Handler`
    - Khởi tạo section `.data` và `.bss`
    - Copy section `.data` từ Flash lên RAM.

    ```c
    /* Copy .data section to SRAM */
	uint32_t size = (uint32_t)&_edata - (uint32_t)&_sdata;
	
	uint8_t *pDst = (uint8_t*)&_sdata;		// SRAM
	uint8_t *pSrc = (uint8_t*)&_etext;	// FLASH
	
	for(uint32_t i = 0; i < size; i++)
	{
		*pDst++ = *pSrc++;
	}
    ```

    - Khởi tạo section `.bss` với giá trị là 0.

    ```c
    /* Init the .bss section to zero in SRAM */
	size = (uint32_t)&_ebss - (uint32_t)&_sbss;
	pDst = (uint8_t*)&_sbss;
	
	for(uint32_t i = 0; i < size; i++)
	{
		*pDst++ = 0;
	}
    ```

    - Khởi tạo standard library (nếu có)

    ```c
    /* Call init function of std libarary */
	__libc_init_array();
    ```

    - Gọi hàm `main`

    ```c
    /* Call main() function */
	main();
    ```