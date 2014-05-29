#include <compex.h>

#define PROPERTY() COMPEX_TAG("property")

struct COMPEX_TAG() SomeClass {
  int x,y;
};

struct COMPEX_TAG("a","b") COMPEX_TAG("c") SomeOtherClass {
  int z,w;
};

struct COMPEX_TAG() SubClass :public SomeClass {
  PROPERTY() int a;

  // direct use of attributes
  [[compex::tag("blah", 42)]] int b;

  virtual int DoSomething(int aa, int bb) {
    return 42;
  }

  COMPEX_TAG("special_method")
  virtual int DoSomething(int aa, int bb) const {
    return 43;
  }

  static int stuff();
  int f1();
  int f2() throw();
};

template<typename T>
struct COMPEX_TAG() TClass {
  int x;
  T y;
};

struct COMPEX_TAG() SubClass2 :public SubClass {
  int g1();
  ~SubClass2();
};

int main(int argc, char **argv) {
  return 0;
}
