#pragma once

#include "FunctionInputDependencyResultInterface.h"
#include "BasicBlocksUtils.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"

namespace input_dependency {

class InputDependentFunctionAnalysisResult final : public FunctionInputDependencyResultInterface
{
public:
    InputDependentFunctionAnalysisResult(llvm::Function* F)
        : m_F(F)
        , m_is_extracted(false)
    {
    }

public:
    llvm::Function* getFunction() override
    {
        return m_F;
    }

    const llvm::Function* getFunction() const override
    {
        return m_F;
    }

    bool isInputDepFunction() const override
    {
        return true;
    }

    void setIsInputDepFunction(bool isInputDep) override
    {
        // Don't allow changing input dep?
    }

    bool isInputDependent(llvm::Instruction* instr) const override
    {
        return true;
    }

    bool isExtractedFunction() const override
    {
        return m_is_extracted;
    }

    void setIsExtractedFunction(bool isExtracted) override
    {
        m_is_extracted = isExtracted;
    }

    bool isInputDependent(const llvm::Instruction* instr) const override
    {
        return true;
    }

    bool isInputIndependent(llvm::Instruction* instr) const override
    {
        return false;
    }

    bool isInputIndependent(const llvm::Instruction* instr) const override
    {
        return false;
    }

    bool isInputDependentBlock(llvm::BasicBlock* block) const override
    {
        return true;
    }

    FunctionSet getCallSitesData() const override
    {
        return FunctionSet();    
    }

    FunctionCallDepInfo getFunctionCallDepInfo(llvm::Function* F) const override
    {
        return FunctionCallDepInfo();
    }

    InputDependentFunctionAnalysisResult* toInputDependentFunctionAnalysisResult() override
    {
        return this;
    }

    long unsigned get_input_dep_blocks_count() const override
    {
        return m_F->getBasicBlockList().size();
    }

    long unsigned get_input_indep_blocks_count() const override
    {
        return 0;
    }

    long unsigned get_unreachable_blocks_count() const override
    {
        return BasicBlocksUtils::get().getFunctionUnreachableBlocksCount(m_F);
    }

    long unsigned get_unreachable_instructions_count() const override
    {
        return BasicBlocksUtils::get().getFunctionUnreachableInstructionsCount(m_F);
    }

    long unsigned get_input_dep_count() const override
    {
        long unsigned count = 0;
        for (auto& B : *m_F) {
            count += B.getInstList().size();
        }
        return count;
    }

    long unsigned get_input_indep_count() const override
    {
        return 0;
    }

    long unsigned get_input_unknowns_count() const override 
    {
        return 0;
    }

private:
    llvm::Function* m_F;
    bool m_is_extracted;
}; // class InputDependentFunctionAnalysisResult

} // namespace input_dependency

