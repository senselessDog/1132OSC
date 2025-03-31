define kernel
  file kernel8.elf
  target remote :1234
  b switch_to_el0
  b sync_lower_el_64_handler
  b sync_lower_el_64_entry
end

define user
  set $user_addr = $arg0
  if $user_addr != 0
    add-symbol-file user/userProcessStatus.elf $user_addr
  else
    file user/userProcessStatus.elf
  end
end