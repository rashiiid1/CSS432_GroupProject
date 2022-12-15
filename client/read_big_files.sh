rm big1.txt
rm big2.txt
rm big3.txt
rm big4.txt
./client -p 12345 -r big1.txt &
./client -p 12345 -r big2.txt &
./client -p 12345 -r big3.txt &
./client -p 12345 -r big4.txt
