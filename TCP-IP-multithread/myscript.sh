function runpa5 {
  local n=4
  for i in `seq 1 $n`; do
    ./client -n 2500 -b 25 -h 127.0.0.1 -r 50001 -p 10 -w 256&
  done
}
