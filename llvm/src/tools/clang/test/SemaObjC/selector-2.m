// RUN: %clang_cc1  -fsyntax-only -Wselector -verify %s
// rdar://8851684
@interface  I
- length;
@end

static inline SEL IsEmpty() {
    return @selector(length);
}

int main (int argc, const char * argv[]) {
    return 0;
}
