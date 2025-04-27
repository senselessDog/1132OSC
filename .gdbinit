define kernel
  file kernel8.elf
  target remote :1234
  # b default_exception_handler
  # b lower_el_irq_exception_handler
  b switch_to_el0
  b handle_syscall
  b process_task_queue
  #b print_str
  b schedule
  b thread_create
  b syscall.c:118
  # b idle
  # b thread_init
  # b thread_create_save
  b thread_exit
  # b buddy_alloc.c:640
  # b buddy_alloc.c:653
end

define user
  set $user_addr = $arg0
  if $user_addr != 0
    add-symbol-file user/userProcessStatus.elf $user_addr
  else
    file user/userProcessStatus.elf
  end
end