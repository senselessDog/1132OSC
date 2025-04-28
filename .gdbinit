define kernel
  file kernel8.elf
  target remote :1234
  # b default_exception_handler
  # b lower_el_irq_exception_handler
  # b switch_to_el0
  # b handle_syscall
  # b process_task_queue
  # b sync_lower_el_64_handler
  # b sync_lower_el_64_entry
  b get_current_thread
  b sys_exec
  # b svc_switch_to
  # b thread_create
  # b exception.S:132
  # b syscall.c:118
  # b svc_load_all
  # b exception.S:128
  # b idle
  # b thread_init
  # b thread_create_save
  b user_thread_exit
end

define user
  set $user_addr = $arg0
  if $user_addr != 0
    add-symbol-file user/userProcessStatus.elf $user_addr
  else
    file user/userProcessStatus.elf
  end
end