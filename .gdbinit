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
  # b get_current
  # b sys_exec
  # b user_idle
  # b sys_signal
  # b sys_kill_signal
  # b signal.c:39
  # b handler_load
  # b sigreturn_trampoline_entry
  # b sys_sigreturn
  #b mmu_setup
  # b *0x80000
  # b _start
  # b mmu_init.S:62
  # b buddy_init
  # b mappages
  # # b walk_and_create_pte
  # b switch_to_el0_vm
  # b *0x0
  # # b sync_lower_el_64_entry
  # b buddy_init
  # b run_user_vm
  # b handle_syscall
  # b syscall.c:35
  # b syscall.c:36
  b sys_fork
  # b el1_irq_entry
  # b lower_el_irq_entry
  # b thread.c:153
  # b thread.c:395
  # b sys_mbox_call
  # b map_framebuffer_for_user
  # b default_exception_handler
  b find_thread_by_pid
  # b check_signals
  b sys_sigreturn
  # b mmu.c:30
  #b simple_alloc
  # b user_thread_schedule
  # b el0_core_timer_enable
  # b lower_el_irq_entry
  # b lower_el_irq_exception_handler
  # b el1_irq_exception_handler
  # b timer_interrupt_handler
  # b svc_switch_to
  # b thread_create
  # b exception.S:132
  # b syscall.c:118
  # b svc_load_all
  # b exception.S:128
  # b idle
  # b thread_init
  # b thread_create_save
  # b user_thread_exit
end

define user
  set $user_addr = $arg0
  if $user_addr != 0
    add-symbol-file user/userProcessStatus.elf $user_addr
  else
    file user/userProcessStatus.elf
  end
end