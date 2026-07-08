# Unit test

These unit tests are for the internal VM machinery.

New tests should always follow these criteria:

1. unit tests should test internal code only, never behaviour, or external facing code
2. unit tests should be very narrow; they should never test more than one function at a time
3. unit tests should have limited scaffolding; when you need a lot of scaffolding code, you are likely testing broader VM behaviour

