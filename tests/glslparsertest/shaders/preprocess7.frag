// [config]
// expect_result: fail
// glsl_version: 1.10
// glsles_version: 1.00
//
// # NOTE: Config section was auto-generated from file
// # NOTE: 'glslparser.tests' at git revision
// # NOTE: 6cc17ae70b70d150aa1751f8e28db7b2a9bd50f0
// [end config]

// testing for char constants in #if and #elif
// Also checking whether reserved words can be redefined.

#define t1 c
#define t2 d
#define asm a

 #if(t1==c)
  #define t3 3
 #elif(t1==d)
  #define t4 4
 #elif(t2==c)
  #define t5 5
 #endif
 
 #ifndef t1
   #define t7 7
 #elif (t2==d)
  #define t6 6
 #endif

 #if (t2=='d')
  #define half 5
 #else
  #define half 8
 #endif

 #ifdef t22
  #define x 5
 #endif

 void main(void)
  {
   int sum =0,a=9;
   
   sum = half + sum;
   sum = asm + a;
   
  }
 
