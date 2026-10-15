export const DEFAULT_CODE = `// AST Studio demo program
int calculate(int a, int b) {
    int result = a + b * 2; // precedence demo
    if (result > 10) {
        result = result - 1;
    } else {
        result = 0;
    }
    return result;
}

int main() {
    int i = 0;
    while (i < 3) {
        i = i + 1;
    }
    return calculate(i, 4);
}
`;

export type Preset = { id: string; title: string; code: string };

export const PRESETS: Preset[] = [
  { id: "default", title: "Demo program", code: DEFAULT_CODE },
  {
    id: "precedence",
    title: "Precedence: a + b * c vs (a + b) * c",
    code: `int a = 1;
int b = 2;
int c = 3;
// * binds tighter than +: the BinaryOp '*' is a child of '+'
int x = a + b * c;
// parentheses flip the tree: '+' becomes a child of '*'
int y = (a + b) * c;
// subtraction is left-associative: (a - b) - c
int z = a - b - c;
int w = a - (b - c);
`,
  },
  {
    id: "control",
    title: "Control flow",
    code: `int main() {
    int total = 0;
    for (int i = 0; i < 10; i = i + 1) {
        if (i > 5) {
            total = total + i;
        } else if (i == 5) {
            total = total * 2;
        } else {
            total = total - 1;
        }
    }
    while (total > 100) total = total / 2; /* body gets braces when pretty-printed */
    return total;
}
`,
  },
  {
    id: "functions",
    title: "Functions",
    code: `float square(float x) {
    return x * x;
}

int add(int a, int b) { return a + b; }

void noop() {
    // nothing here
}

int main() {
    float area = square(2.5);
    int sum = add(add(1, 2), -3);
    noop();
    return sum;
}
`,
  },
  {
    id: "syntax-error",
    title: "Syntax error (missing ;)",
    code: `int main() {
    int x = 10
    return x;
}
`,
  },
];
