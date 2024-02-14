int foo(int a, int b) {
    if (a <5) {
        for(int i=0; i<5; i++)
            b += 1;
    }
    else
        b *= a;
    return b;
}

int main(int argc, char **argv) {
    int x = foo (argc, argc*32478);
    return x;
}