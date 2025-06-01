define kernel
  file kernel8.elf
  target remote :1234
  # b vfs_open
  b vfs.c:658
  b tmpfs_create_common
end

define user
  set $user_addr = $arg0
  if $user_addr != 0
    add-symbol-file user/userProcessStatus.elf $user_addr
  else
    file user/userProcessStatus.elf
  end
end