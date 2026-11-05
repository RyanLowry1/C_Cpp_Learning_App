MALLOC'S LIBRARY - CONTENT FORMAT
=================================

All learning content lives in plain text files in this folder, so new
questions, chapters and coding problems can be added without touching the
C++ source. Run the self-test after any change:

    MallocsLibrary.exe --selftest            (everything)
    MallocsLibrary.exe --selftest p.2        (only ids containing "p.2")

The self-test compiles and runs every model solution against its tests,
checks every starter compiles, and runs every "predict the output" question
to prove its answer key is what the program really prints. The report is also
written to save/selftest.txt.

Topic ids (used by "topic:" everywhere):
    Exam 1:  basics  control  arrays  functions
    Exam 2:  pointers  memory  linkedlist  classes
    C++:     operators  templates  vectors  listsmaps


1. RECORD FILES (mcq.txt, written.txt, coding/*.txt)
----------------------------------------------------
A file is a list of records. A record starts with "@kind id". Each following
line is "key: value". A key with nothing after the colon starts a verbatim
block that runs until a line containing only "@end". Lines starting with '#'
(outside blocks) are comments.

In single-line values (q:, a:, why:, ans: ...) \n means a new line and \\
is a backslash, so a literal backslash-n is written \\n. Inline formatting:
**bold**, __italic__ and `code`.


2. MULTIPLE CHOICE - mcq.txt
----------------------------
    @q p.05
    topic: pointers
    diff: 2                     (1 easy, 2 medium, 3 hard; default 1)
    q: What does this print?
    code:
    int x = 15;
    int *p = &x;
    printf("%d\n", *p);
    @end
    a: The address of x         (a wrong option)
    *a: 15                      (the correct option - exactly one)
    a: 0
    why: Shown after answering, right or wrong.
    verify: output              (optional, see below)

Options are shuffled each sitting. "verify: output" makes the self-test run
the code and check the correct option equals the program's output. Code
without a main() is wrapped in one automatically, with the usual C and C++
headers and "using namespace std;".


3. WRITTEN QUESTIONS - written.txt
----------------------------------
    @w wp.01
    topic: pointers
    kind: output                (output | blank | fix | concept)
    lines: multi                (optional: multi-line answer box)
    q: Trace i, j and p. What is printed?
    code:
    ...
    @end
    ans: 30 5                   (repeat ans: for alternative answers)
    why: Explanation shown after checking.
    run: yes                    (self-test runs the code; output must equal the first ans)

How answers are marked:
    output   - case-insensitive, runs of spaces collapsed; spacing and new
               lines are forgiven.
    blank    - all whitespace ignored, trailing ';' ignored. Mark the gap in
    fix        the code with ____ (four underscores).
    concept  - correct if the answer CONTAINS any ans: keyword.
The student can always overrule a "Not quite" with "My answer means the same".


4. READING CHAPTERS - reading/NN_topic.txt
------------------------------------------
Files are loaded in name order. Markup, one item per line:

    @title Pointers
    @topic pointers             (defaults to the part of the file name after '_')
    @subtitle One line under the title
    # Heading                   (listed in the chapter's contents page)
    ## Sub-heading
    Plain lines form paragraphs (a blank line ends a paragraph).
    - bullet point
    > tip callout
    !> warning ("Careful") callout
    *> "Exam focus" callout
    ===                         (preferred page break)
    ```                         (code block, syntax highlighted)
    ```output                   (console output block)

Memory diagrams - one row per box: name | type | value | address | points-to

    @mem
    x | int | 10 | 1000
    p | int* | 1000 | 2000 | x
    @end
    caption: p holds the address of x.

A box whose points-to names another box gets a red arrow. Pages are laid out
automatically; code blocks longer than a page are split.


5. CODING WORKBOOKS - coding/*.txt
----------------------------------
    @set exam2
    title: Coding Exam 2 Workbook
    subtitle: ...
    desc: Introduction shown on the workbook's first page.
    prelude: cpp                (c = C-style headers, cpp = C++ headers)
    exam: 2                     (1 or 2 adds a timed Mock Exam; 0 = none)
    color: 120,40,60            (cover colour)

    @p e2.swap                  (problems belong to the @set above them)
    title: Swap Two Integers
    topic: pointers
    diff: Easy                  (Easy | Medium | Hard)
    desc:
    The problem statement (markup as in paragraphs above).
    @end
    example:
    swap(&a, &b)  ->  a and b exchanged
    @end
    support:                    (optional: given code compiled before the
    struct Node { ... };         student's code, e.g. struct definitions)
    @end
    helpers:                    (optional: code available to the tests only)
    @end
    starter:
    void swap(int *a, int *b) {
        // your code here
    }
    @end
    tests:
    + a sample test, shown and run by Run
    EXPECT_EQ(...);
    - a hidden test, only run by Submit
    EXPECT_EQ(...);
    @end
    forbid: <<|>> ; Message shown if the student's code uses a forbidden token.
    hint: One-line nudge.
    solution:
    ...model solution, revealed on request...
    @end

Test macros (each test may contain several statements):
    EXPECT_EQ(actual, expected)      EXPECT_TRUE(x)   EXPECT_FALSE(x)
    EXPECT_NEAR(actual, expected, tolerance)
    EXPECT_STR(actual, "expected")   (C strings; use .c_str() for a std::string)
    EXPECT_ARR(array, n, {values})
    EXPECT_SIGN(actual, s)           (s is -1, 0 or 1 - for strcmp-style results)
    EXPECT_OUT(call, "printed text") (captures stdout exactly)
    EXPECT_OUT_WS(call, "text")      (same, ignoring spacing differences)

Each test runs in the student's program with a time limit; an infinite loop or
crash (null pointer, out of bounds) fails that test with a readable message
instead of taking the app down.
