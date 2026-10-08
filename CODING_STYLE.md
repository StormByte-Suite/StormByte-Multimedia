# StormByte coding style

This is how Base is written. Other suite modules follow it unless their own file says otherwise. If this file is silent, copy the nearest file that already does the thing you are doing.

## Files

C++ headers are `.hxx`. Sources are `.cxx`. Template bodies that would dirty the header go in a `.txx`, included at the bottom of the header. A private C helper is `.h` and `.c`, compiled as C, and is not installed.

Every C and C++ file starts with the license banner already used in the tree, unchanged, then `#pragma once` on a header. CMake and Markdown do not take the banner.

Includes are the project's own headers, alphabetical, a blank line, then the standard library, alphabetical. A header does not contain `using namespace`. A `.cxx` may say `using namespace StormByte;` after the includes, or name the few symbols it actually uses.

## Layout

Indent with tabs. A space used to indent is wrong. Do not pad with spaces to line columns up. A trailing `///<` on a member may be tabbed to the same column as its neighbours, and is dropped when the name is too long to keep that column.

Braces are K&R. The `{` stays on the line of `class`, `struct`, `enum`, `namespace`, `if`, `for`, `while` and the function signature. `public:` and `private:` are one tab in. Members are one more. A `friend` sits with the members of the section that owns it, not before the first visibility label.

A single-statement `if`, `else` or `else if` has no braces. `else` and `else if` go on their own line.

```cpp
if (unit == 0 || remainder == 0)
	std::snprintf(...);
else {
	...
}
```

`*` and `&` bind to the type: `const char* text`, `String& other`. Not `char *text`.

One statement per line. One empty line between members. No anonymous namespace in a public header.

## Names

Types, enumerations and functions are PascalCase: `Base64Encode`, `Length`, `Fault`. Macros are `SCREAMING_SNAKE`: `STORMBYTE_PUBLIC`, `WINDOWS`.

A data member is snake case and starts with `m_`: `m_thread_count`. A constant is PascalCase or full uppercase. Google `kConstant` is not used.

```cpp
constexpr int Max = 5;
constexpr int THRESHOLD = 5;
```

Snake case is reserved for a type that is deliberately std-like. `Safe::String::push_back` stays snake case because the point of that type is that a reader already knows the name. A helper that is not part of that emulated surface is PascalCase, including a private one. Do not mix the two on the same surface.

A test function is snake case and starts with `test_`. Assert macros take the name from `__func__`. Do not pass the function name, and do not repeat it as a string literal.

An accessor does not take a `Get` or `Set` prefix. The getter is the PascalCase name. The setter is the same name with an argument.

```cpp
Foo::Data();
Foo::Data(int value);
```

## Classes

A class that owns or is copied is written in canonical order, and none of those six is omitted:

1. constructor
2. copy constructor
3. move constructor
4. destructor
5. copy assignment
6. move assignment

`= default` and `= delete` are written out in that position. They are not left for the compiler to invent. Extra constructors go with the first constructor, not between the assignment operators.

A converting constructor is `explicit` unless the type documents an implicit conversion. `Safe::String` to `std::string_view` is that case. A conversion to an STL container is explicit.

Mark `noexcept` only when it is true. Prefer `constexpr` when there is no heap and no I/O. A function that builds a `Safe::String` inside the library is not `constexpr`.

```cpp
/**
 * @class Foo
 * @brief Sample owner used by the style guide. It does not cross a DLL.
 */
class STORMBYTE_PUBLIC Foo {
	public:
		/**
		 * @brief Construct an invalid object with no workers.
		 */
		Foo();

		/**
		 * @brief Copy an object.
		 * @param other Source.
		 */
		Foo(const Foo& other);

		/**
		 * @brief Take an object. @p other is left invalid.
		 * @param other Source.
		 */
		Foo(Foo&& other) noexcept;

		/**
		 * @brief Destroy the object.
		 */
		virtual ~Foo();

		/**
		 * @brief Copy-assign an object.
		 * @param other Source.
		 * @return This object.
		 */
		Foo& operator=(const Foo& other);

		/**
		 * @brief Move-assign an object. @p other is left invalid.
		 * @param other Source.
		 * @return This object.
		 */
		Foo& operator=(Foo&& other) noexcept;

		/**
		 * @brief Report whether this object can be used.
		 * @return Whether the object is valid.
		 */
		bool Valid() const { return m_valid; }

		/**
		 * @brief Set whether this object can be used.
		 * @param valid New validity.
		 */
		void Valid(bool valid) { m_valid = valid; }

		/**
		 * @brief Return the worker count.
		 * @return Worker count.
		 */
		int ThreadCount() const { return m_thread_count; }

		/**
		 * @brief Set the worker count.
		 * @param threads Worker count. Not checked.
		 */
		void ThreadCount(int threads) { m_thread_count = threads; }

	private:
		bool m_valid;			///< Whether the object can be used.
		int m_thread_count;		///< Worker count. Zero until set.
};
```

## Namespaces

Nest namespaces in the header when there are three or fewer. Each opened namespace gets the same `@namespace` and `@brief` block. Do not close with `// namespace Foo` or a similar tag.

## Language

The dialect is C++26. New code does not call bare `new` or `delete`. Owned memory goes through `Safe::Heap`, or through a Safe owner.

`enum class` only. Platform tests are `#ifdef WINDOWS`, `#elifdef MACOS`, `#else`. Not `#if defined(WINDOWS)` and not raw `_WIN32`.

A precondition failure, such as `operator[]` out of range, is undefined and `assert` when assertions are on. Do not throw for it. A checked access that is part of the type's contract throws a StormByte exception.

## Concepts

Constrain a public template with `StormByte::Type`. `Type::SameAs`, not `std::same_as`. Do not put `std::enable_if`, `void_t` or a raw `std::is_*` next to a concept that already says the same thing.

## DLL boundary

`STORMBYTE_PUBLIC` comes first on a function declaration. clang-cl rejects `__declspec` after a reference return type.

```cpp
STORMBYTE_PUBLIC Safe::String GenerateUUIDv4() noexcept;
STORMBYTE_PUBLIC const Category<Code>& category() noexcept;
```

Do not write `Safe::String STORMBYTE_PUBLIC Foo();`. On a class the attribute stays on the type: `class STORMBYTE_PUBLIC Fault`. Do not repeat it on an ordinary `.cxx` definition.

An exported template is split. The header has the `extern template` with `STORMBYTE_PUBLIC`. The `.cxx` has the body with `STORMBYTE_INSTANTIATE`. Never put `STORMBYTE_INSTANTIATE` on the `extern` line, and never write `STORMBYTE_PUBLIC extern template`.

A value that leaves the shared library is a Safe type, or a borrowed `const char*` owned by this library. It is not a `std::string`, a `std::vector` or a `std::function`. A conversion to one of those is `STORMBYTE_FORCE_INLINE` in the header, so the STL object is born in the caller. A private C helper is not exported.

## Doxygen

Every declaration a reader can trip over is documented: types, functions, data members, template parameters. Each one gets its own block. Do not compress several functions into one block, and do not collapse a function block onto one line.

A function gets `@brief`, and `@param`, `@return` or `@throws` when it has them. `@return` names the value, not the C++ type. `@ref` uses the qualified name. A comment says what the thing is for. It does not repeat the signature.

`= delete` does not need a paragraph. A data member whose name already says what it is takes a trailing `///<`. Large classes may use `@name` groups. Wrap a list of `extern template` in `/// @cond` and `/// @endcond` so it does not become a page of instantiations.

## Tests

Sections are alphabetical. Functions inside a section are alphabetical. The banner is the same in the test body and in `main`, and `main` groups the calls under it:

```cpp
// -------------------
// Construct
// -------------------
```

The executable name in CMake is PascalCase. The source is `<component>_test.cxx`.

## Commits

Conventional Commits, in English: `feat:`, `fix:`, `docs:`, `test:`, `refactor:`. A breaking change takes `!`. One topic per commit. The subject says what changed, not the file that changed.
