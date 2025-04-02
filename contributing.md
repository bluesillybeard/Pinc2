# Contributing to Pinc

Want to contribute to Pinc? Here's what you need to know.

## Prerequisites
- A usable understanding of the Pinc API
- A decent understanding of cmake - we use cmake for the development of Pinc itself, even if it supports many more build systems
- A supported compiler
- Word of approval from one of these people, either through an issue or direct communication:
    - bluesillybeard2
- A good understanding of Git and Github
- decent manners - we don't tolerate misbehavior, although those who would misbehave probably won't read this anyway

The reason we ask for you to get approval is to avoid wasting time. We don't want you to spend your valuable time working on something that will ultimately be rejected.

Even with approval, it is not guaranteed that we will accept your PR. It is the reality of open-source development that not all code can be accepted. Don't worry though, if your project / idea is approved, we will point you towards making your PR acceptable - unless another better one comes along.

## Code Style

*Externally* (so in user-facing headers), the code style is determined and must be adhered to:
- types are in `PascalCase`
- arguments are in `snake_case`
- functions are `camelCase`
- No macros, of any kind, no exceptions

By contrast, the *internal* Pinc code is an utter mess. It's all crap quality code, in terms of style conventions anyway. So, unless you're trying to make it worse, pretty much anything goes. The plan is to fix up the internal style conventions at some point - but for now, it remains a bit of a disaster.

## Actually modifying the code

- Forking from `main` is your best bet, as other branches are in-progress development branches that are likely to be broken or messy.
- In your own fork, make your changes. Try to keep them small, we don't want huge refactor PR's if possible.
- Submit the PR through Github, or in private if that is desired.
- We'll go over your changes, and submit any requests or advice. We don't do nitpicks, those can be fixed later.
- We will also run your PR through the testing suite - which for now is just manually running every example in an appropriate environment. Eventually, we will have a proper CI system set up.

## Merge requirements

Your PR is ready to merge once these requirements are met:
- tested and works for all relevant platforms and build systems
- most discussion threads are resolved
    - What counts as most is really a case-by-case thing, and ultimately up to the Pinc developers
- the PR is marked as ready
- the PR is approved by a Pinc developer
