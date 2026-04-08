// PR c++/124794
// { dg-do compile { target c++26 } }
// { dg-additional-options "-freflection -Wno-error=missing-template-keyword" }

struct C { template<class T> void f(T); };
void (C::*p1)(int) = &template[:^^C::f:];
void (C::*p2)(int) = &[:^^C::f:]; // { dg-warning "keyword before dependent template name" }
void (C::*p3)(int) = &template[:^^C::f:]<int>;
void (C::*p4)(int) = &[:^^C::f:]<int>; // { dg-warning "keyword before dependent template name" }
auto p5 = &template[:^^C::f:]<int>;
auto p6 = &[:^^C::f:]<int>; // { dg-warning "keyword before dependent template name" }

struct Base1{
  template<class T>
  constexpr T f(T x) {
    return x;
  }
};
struct Base2: Base1 {
  template<class T>
  constexpr T g(T x) {
    return x;
  }
};
struct Base3: Base1, Base2 {}; // { dg-warning "inaccessible" }

static_assert(Base1{}.[:^^Base1::f:](13)==13);
static_assert(Base1{}.[:^^Base1::f:]<int>(13)==13);
static_assert(Base3{}.[:^^Base2::g:](13)==13);
static_assert(Base3{}.[:^^Base2::g:]<int>(13)==13);
constexpr int invalid1 = Base3{}.[:^^Base2::f:]; // { dg-error "ambiguous" }
constexpr int invalid2 = Base3{}.[:^^Base1::f:]; // { dg-error "ambiguous" }