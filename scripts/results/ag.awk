# 2263 1 336344
# second interface bytes

BEGIN {
	sum0=0;
	cnt0=0; 
	sum1=0;
	cnt1=0; 
	sec=0;
} 

{
	if ($1 != sec) 
	{
		avg0=sum0/cnt0;
		avg1=sum1/cnt1;
		if (avg0>=avg1){
			fid=avg1/avg0;
		} else {
			fid=avg0/avg1;
		}
		print sec " " sum0 " " cnt0 " " avg0 " " sum1 " " cnt1 " "  avg1 "     " fid ;
		sec=$1;
		sum0=0; cnt0=0;
		sum1=0; cnt1=0;
	}
	if($2==0)
	{
		cnt0++;
		sum0+=$3;
	} else {
		cnt1++;
		sum1+=$3
	}
}
