
#include<iostream>
using namespace std;
int main()
{
    int palindrome = 2345432, reverse=0;
    cout<<"Number: 2345432 ";
    //cin>>palindrome;
    int num=0,key = palindrome;
    for(int i=1;palindrome!=0;i++){
        
        num=palindrome%10;
        palindrome=palindrome/10;
        reverse=num+(reverse*10);
    }
    
    if(reverse==key){
        cout<<key<<" is a Palindrome Number";
    }
    else{
        cout<<key<<"is NOT a Palindrome Number";
    }
    return 0;
}

