make
sudo rmmod barstool.ko
sudo insmod barstool.ko
# open bar
./a.out 1
echo ----- initial state
cat ../../../../proc/majorsbar
# a.out 2 (num) (type) to summon customer. 0 freshman, 1 sophomore, 2 junior, 3 senior, 4 prof / grad
./a.out 2 $RANDOM % 8 $RANDOM % 5
echo ----- state after 2 sophomore
cat ../../../../proc/majorsbar
./a.out 2 $RANDOM % 8 $RANDOM % 5
echo ----- state after 4 profs
cat ../../../../proc/majorsbar
# close bar
./a.out 3
echo ----- closed state
cat ../../../../proc/majorsbar
