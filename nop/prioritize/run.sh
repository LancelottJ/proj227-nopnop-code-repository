
#!/bin/bash
/home/femu/test4free/a.out & 
/home/femu/test4free/b.out &
/home/femu/test4free/c.out &
pida=$(pgrep a.out)
pidb=$(pgrep b.out)

sudo ./atc pid $pida tgid $pida
echo a:$pida b:$pidb