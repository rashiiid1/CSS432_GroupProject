rm ../server/big1.txt
rm ../server/big3.txt
rm big2.txt
rm big4.txt
./client -p 12345 -w big1.txt &
./client -p 12345 -r big2.txt &
./client -p 12345 -w big3.txt &
./client -p 12345 -r big4.txt
