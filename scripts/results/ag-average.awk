# Input lines:
# 2263 1 336344
# ie: second interface bytes

BEGIN {
	sum=0;
	cnt=0; 
} 

{
	sum += $8;
	cnt ++;
}

END {
	print sum/cnt;
}
