// cyclomatic complexity: +1
int sumOfPrimes(int max) {
  int total = 0;
  OUT:  
  // cyclomatic complexity: +1
  // cognitive complexity: +1, nesting level is now 1
  for (int i = 1; i <= max; ++i) {
    // cyclomatic complexity: +1
    // cognitive complexity: +2 (nesting=1), nesting level is now 2
    for  (int j = 2; j < i; ++j) {
      // cyclomatic complexity: +1
      // cognitive complexity: +3 (nesting=2), nesting level is now 3
      if (i % j == 0) {
        continue OUT;
      }
    }
    total += i;
  }
  return total;
}
// cyclomatic complexity: 4
// cognitive complexity: 6

// cyclomatic complexity: +1
String getWords(int number) {
  // cognitive complexity: +1, nesting level is now 1
  switch (number) {
    // cyclomatic complexity: +1
    case 1:
      return "one";
    // cyclomatic complexity: +1
    case 2:
      return "a couple";
    // cyclomatic complexity: +1
    case 3:
      return "a few";
    default:
      return "lots";
  }
}
// cyclomatic complexity: 4
// cognitive complexity: 1
