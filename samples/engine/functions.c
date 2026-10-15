int calculate(int a, int b) {
    int result = a + b * 2;
    return result;
}
int counter = 0;
void tick() { counter = counter + 1; return; }
float half(float v){return v/2.0;}
int main() {
    tick();
    return calculate(10, 20);
}
