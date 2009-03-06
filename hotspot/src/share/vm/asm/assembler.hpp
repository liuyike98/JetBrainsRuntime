/*
 * Copyright 1997-2006 Sun Microsystems, Inc.  All Rights Reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa Clara,
 * CA 95054 USA or visit www.sun.com if you need additional information or
 * have any questions.
 *
 */

// This file contains platform-independent assembler declarations.

class CodeBuffer;
class MacroAssembler;
class AbstractAssembler;
class Label;

/**
 * Labels represent destinations for control transfer instructions.  Such
 * instructions can accept a Label as their target argument.  A Label is
 * bound to the current location in the code stream by calling the
 * MacroAssembler's 'bind' method, which in turn calls the Label's 'bind'
 * method.  A Label may be referenced by an instruction before it's bound
 * (i.e., 'forward referenced').  'bind' stores the current code offset
 * in the Label object.
 *
 * If an instruction references a bound Label, the offset field(s) within
 * the instruction are immediately filled in based on the Label's code
 * offset.  If an instruction references an unbound label, that
 * instruction is put on a list of instructions that must be patched
 * (i.e., 'resolved') when the Label is bound.
 *
 * 'bind' will call the platform-specific 'patch_instruction' method to
 * fill in the offset field(s) for each unresolved instruction (if there
 * are any).  'patch_instruction' lives in one of the
 * cpu/<arch>/vm/assembler_<arch>* files.
 *
 * Instead of using a linked list of unresolved instructions, a Label has
 * an array of unresolved instruction code offsets.  _patch_index
 * contains the total number of forward references.  If the Label's array
 * overflows (i.e., _patch_index grows larger than the array size), a
 * GrowableArray is allocated to hold the remaining offsets.  (The cache
 * size is 4 for now, which handles over 99.5% of the cases)
 *
 * Labels may only be used within a single CodeSection.  If you need
 * to create references between code sections, use explicit relocations.
 */
class Label VALUE_OBJ_CLASS_SPEC {
 private:
  enum { PatchCacheSize = 4 };

  // _loc encodes both the binding state (via its sign)
  // and the binding locator (via its value) of a label.
  //
  // _loc >= 0   bound label, loc() encodes the target (jump) position
  // _loc == -1  unbound label
  int _loc;

  // References to instructions that jump to this unresolved label.
  // These instructions need to be patched when the label is bound
  // using the platform-specific patchInstruction() method.
  //
  // To avoid having to allocate from the C-heap each time, we provide
  // a local cache and use the overflow only if we exceed the local cache
  int _patches[PatchCacheSize];
  int _patch_index;
  GrowableArray<int>* _patch_overflow;

  Label(const Label&) { ShouldNotReachHere(); }

 public:

  /**
   * After binding, be sure 'patch_instructions' is called later to link
   */
  void bind_loc(int loc) {
    assert(loc >= 0, "illegal locator");
    assert(_loc == -1, "already bound");
    _loc = loc;
  }
  void bind_loc(int pos, int sect);  // = bind_loc(locator(pos, sect))

#ifndef PRODUCT
  // Iterates over all unresolved instructions for printing
  void print_instructions(MacroAssembler* masm) const;
#endif // PRODUCT

  /**
   * Returns the position of the the Label in the code buffer
   * The position is a 'locator', which encodes both offset and section.
   */
  int loc() const {
    assert(_loc >= 0, "unbound label");
    return _loc;
  }
  int loc_pos() const;   // == locator_pos(loc())
  int loc_sect() const;  // == locator_sect(loc())

  bool is_bound() const    { return _loc >=  0; }
  bool is_unbound() const  { return _loc == -1 && _patch_index > 0; }
  bool is_unused() const   { return _loc == -1 && _patch_index == 0; }

  /**
   * Adds a reference to an unresolved displacement instruction to
   * this unbound label
   *
   * @param cb         the code buffer being patched
   * @param branch_loc the locator of the branch instruction in the code buffer
   */
  void add_patch_at(CodeBuffer* cb, int branch_loc);

  /**
   * Iterate over the list of patches, resolving the instructions
   * Call patch_instruction on each 'branch_loc' value
   */
  void patch_instructions(MacroAssembler* masm);

  void init() {
    _loc = -1;
    _patch_index = 0;
    _patch_overflow = NULL;
  }

  Label() {
    init();
  }
};


// The Abstract Assembler: Pure assembler doing NO optimizations on the
// instruction level; i.e., what you write is what you get.
// The Assembler is generating code into a CodeBuffer.
class AbstractAssembler : public ResourceObj  {
  friend class Label;

 protected:
  CodeSection* _code_section;          // section within the code buffer
  address      _code_begin;            // first byte of code buffer
  address      _code_limit;            // first byte after code buffer
  address      _code_pos;              // current code generation position
  OopRecorder* _oop_recorder;          // support for relocInfo::oop_type

  // Code emission & accessing
  address addr_at(int pos) const       { return _code_begin + pos; }

  // This routine is called with a label is used for an address.
  // Labels and displacements truck in offsets, but target must return a PC.
  address target(Label& L);            // return _code_section->target(L)

  bool is8bit(int x) const             { return -0x80 <= x && x < 0x80; }
  bool isByte(int x) const             { return 0 <= x && x < 0x100; }
  bool isShiftCount(int x) const       { return 0 <= x && x < 32; }

  void emit_byte(int x);  // emit a single byte
  void emit_word(int x);  // emit a 16-bit word (not a wordSize word!)
  void emit_long(jint x); // emit a 32-bit word (not a longSize word!)
  void emit_address(address x); // emit an address (not a longSize word!)

  // Instruction boundaries (required when emitting relocatable values).
  class InstructionMark: public StackObj {
   private:
    AbstractAssembler* _assm;

   public:
    InstructionMark(AbstractAssembler* assm) : _assm(assm) {
      assert(assm->inst_mark() == NULL, "overlapping instructions");
      _assm->set_inst_mark();
    }
    ~InstructionMark() {
      _assm->clear_inst_mark();
    }
  };
  friend class InstructionMark;
  #ifdef ASSERT
  // Make it return true on platforms which need to verify
  // instruction boundaries for some operations.
  inline static bool pd_check_instruction_mark();
  #endif

  // Label functions
  void print(Label& L);

 public:

  // Creation
  AbstractAssembler(CodeBuffer* code);

  // save end pointer back to code buf.
  void sync();

  // ensure buf contains all code (call this before using/copying the code)
  void flush();

  // Accessors
  CodeBuffer*   code() const;          // _code_section->outer()
  CodeSection*  code_section() const   { return _code_section; }
  int           sect() const;          // return _code_section->index()
  address       pc() const             { return _code_pos; }
  int           offset() const         { return _code_pos - _code_begin; }
  int           locator() const;       // CodeBuffer::locator(offset(), sect())
  OopRecorder*  oop_recorder() const   { return _oop_recorder; }
  void      set_oop_recorder(OopRecorder* r) { _oop_recorder = r; }

  address  inst_mark() const;
  void set_inst_mark();
  void clear_inst_mark();

  // Constants in code
  void a_byte(int x);
  void a_long(jint x);
  void relocate(RelocationHolder const& rspec, int format = 0);
  void relocate(   relocInfo::relocType rtype, int format = 0) {
    if (rtype != relocInfo::none)
      relocate(Relocation::spec_simple(rtype), format);
  }

  static int code_fill_byte();         // used to pad out odd-sized code buffers

  // Associate a comment with the current offset.  It will be printed
  // along with the disassembly when printing nmethods.  Currently
  // only supported in the instruction section of the code buffer.
  void block_comment(const char* comment);

  // Label functions
  void bind(Label& L); // binds an unbound label L to the current code position

  // Move to a different section in the same code buffer.
  void set_code_section(CodeSection* cs);

  // Inform assembler when generating stub code and relocation info
  address    start_a_stub(int required_space);
  void       end_a_stub();
  // Ditto for constants.
  address    start_a_const(int required_space, int required_align = sizeof(double));
  void       end_a_const();

  // fp constants support
  address double_constant(jdouble c) {
    address ptr = start_a_const(sizeof(c), sizeof(c));
    if (ptr != NULL) {
      *(jdouble*)ptr = c;
      _code_pos = ptr + sizeof(c);
      end_a_const();
    }
    return ptr;
  }
  address float_constant(jfloat c) {
    address ptr = start_a_const(sizeof(c), sizeof(c));
    if (ptr != NULL) {
      *(jfloat*)ptr = c;
      _code_pos = ptr + sizeof(c);
      end_a_const();
    }
    return ptr;
  }
  address address_constant(address c, RelocationHolder const& rspec) {
    address ptr = start_a_const(sizeof(c), sizeof(c));
    if (ptr != NULL) {
      relocate(rspec);
      *(address*)ptr = c;
      _code_pos = ptr + sizeof(c);
      end_a_const();
    }
    return ptr;
  }
  inline address address_constant(Label& L);
  inline address address_table_constant(GrowableArray<Label*> label);

  // Bang stack to trigger StackOverflowError at a safe location
  // implementation delegates to machine-specific bang_stack_with_offset
  void generate_stack_overflow_check( int frame_size_in_bytes );
  virtual void bang_stack_with_offset(int offset) = 0;


  /**
   * A platform-dependent method to patch a jump instruction that refers
   * to this label.
   *
   * @param branch the location of the instruction to patch
   * @param masm the assembler which generated the branch
   */
  void pd_patch_instruction(address branch, address target);

#ifndef PRODUCT
  /**
   * Platform-dependent method of printing an instruction that needs to be
   * patched.
   *
   * @param branch the instruction to be patched in the buffer.
   */
  static void pd_print_patched_instruction(address branch);
#endif // PRODUCT
};

#include "incls/_assembler_pd.hpp.incl"
