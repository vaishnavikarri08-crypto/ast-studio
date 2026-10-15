// File header comment
/* block header */
int x = 1; // trailing on x
int y = x + /* inside expr */ 2;
/* two */ /* on one line */
int add(int a, int b) { // first line of body
    // before return
    return a + b; /* after return */
    // dangling in body
}
if (x > 0) x = 0; // after then
else {
    y = 1;
}
while (y) {
    y = y - 1; // dec
    /* end of loop body */
}
// end of file
