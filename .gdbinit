define kernel
  file kernel8.elf
  target remote :1234
  # b async_io.c:12
  # b uart_async_send_string
  # b async_io.c:17
  # b default_exception_handler
  # b irq_exception_handler
  # b irq_entry
  # b uart_async_send_string
  # b is_uart_interrupt
  # b uart_irq_handler
  b timer_interrupt_handler
end

define user
  set $user_addr = $arg0
  if $user_addr != 0
    add-symbol-file user/userProcessStatus.elf $user_addr
  else
    file user/userProcessStatus.elf
  end
end