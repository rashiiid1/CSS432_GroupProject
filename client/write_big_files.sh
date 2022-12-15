rm ../server/big1.txt
rm ../server/big2.txt
rm ../server/big3.txt
rm ../server/big4.txt

./client -p 12345 -w big1.txt &
./client -p 12345 -w big2.txt &
./client -p 12345 -w big3.txt &
./client -p 12345 -w big4.txt
ll