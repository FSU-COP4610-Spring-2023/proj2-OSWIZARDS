make
sudo rmmod barstool.ko
sudo insmod barstool.ko
./a.out 1
cat ../../../../proc/majorsbar
# a.out 2 (num) (type) to summon customer. 0 freshman, 1 sophomore, 2 junior, 3 senior, 4 prof / grad
./a.out 2 1 1
cat ../../../../proc/majorsbar
./a.out 2 4 4
cat ../../../../proc/majorsbar
./a.out 3
