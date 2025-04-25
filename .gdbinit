define kernel
  file kernel8.elf
  target remote :1234
  b thread_create
  # b idle
  b thread_init
  b thread_create_save
  b schedule
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