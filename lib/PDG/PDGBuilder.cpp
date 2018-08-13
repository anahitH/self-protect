#include "PDG/PDGBuilder.h"

#include "PDG/PDGEdge.h"
#include "PDG/DefUseResults.h"
#include "analysis/IndirectCallSiteResults.h"

#include "llvm/Analysis/MemorySSA.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

namespace pdg {

PDGBuilder::PDGBuilder(llvm::Module* M,
                       DefUseResultsTy pointerDefUse,
                       DefUseResultsTy scalarDefUse,
                       IndCSResultsTy indCSResults)
    : m_module(M)
    , m_ptDefUse(pointerDefUse)
    , m_scalarDefUse(scalarDefUse)
    , m_indCSResults(indCSResults)
{
}

void PDGBuilder::build()
{
    m_pdg.reset(new PDG(m_module));
    visitGlobals();

    for (auto& F : *m_module) {
        if (F.isDeclaration()) {
            buildFunctionDefinition(&F);
        }
        buildFunctionPDG(&F);
        m_currentFPDG.reset();
    }
}

void PDGBuilder::visitGlobals()
{
    for (auto glob_it = m_module->global_begin();
            glob_it != m_module->global_end();
            ++glob_it) {
        m_pdg->addGlobalVariableNode(&*glob_it);
    }
}

PDGBuilder::FunctionPDGTy PDGBuilder::buildFunctionDefinition(llvm::Function* F)
{
    FunctionPDGTy functionPDG = FunctionPDGTy(new FunctionPDG(F));
    m_pdg->addFunctionPDG(F, functionPDG);
    visitFormalArguments(functionPDG, F);
}

void PDGBuilder::buildFunctionPDG(llvm::Function* F)
{
    if (!m_pdg->hasFunctionPDG(F)) {
        m_currentFPDG.reset(new FunctionPDG(F));
        m_pdg->addFunctionPDG(F, m_currentFPDG);
    } else {
        m_currentFPDG = m_pdg->getFunctionPDG(F);
    }
    if (!m_currentFPDG->isFunctionDefBuilt()) {
        visitFormalArguments(m_currentFPDG, F);
    }
    for (auto& B : *F) {
        visitBlock(B);
        visitBlockInstructions(B);
    }
}

void PDGBuilder::visitFormalArguments(FunctionPDGTy functionPDG, llvm::Function* F)
{
    for (auto arg_it = F->arg_begin();
            arg_it != F->arg_end();
            ++arg_it) {
        functionPDG->addFormalArgNode(&*arg_it);
    }
    functionPDG->setFunctionDefBuilt(true);
}

void PDGBuilder::visitBlock(llvm::BasicBlock& B)
{
    m_currentFPDG->addNode(llvm::dyn_cast<llvm::Value>(&B),
            PDGNodeTy(new PDGLLVMBasicBlockNode(&B)));
}

void PDGBuilder::visitBlockInstructions(llvm::BasicBlock& B)
{
    for (auto& I : B) {
        visit(I);
    }
    addControlEdgesForBlock(B);
}

void PDGBuilder::addControlEdgesForBlock(llvm::BasicBlock& B)
{
    if (!m_currentFPDG->hasNode(&B)) {
        return;
    }
    auto blockNode = m_currentFPDG->getNode(&B);
    // Don't add control edges if block is not conreol dependent on something
    if (blockNode->getInEdges().empty()) {
        return;
    }
    for (auto& I : B) {
        if (!m_currentFPDG->hasNode(&I)) {
            continue;
        }
        auto destNode = m_currentFPDG->getNode(&I);
        addControlEdge(blockNode, destNode);
    }
}

void PDGBuilder::visitBranchInst(llvm::BranchInst& I)
{
    // TODO: output this for debug mode only
    llvm::dbgs() << "Branch Inst: " << I << "\n";
    if (I.isConditional()) {
        llvm::Value* cond = I.getCondition();
        if (auto sourceNode = getNodeFor(cond)) {
            auto destNode = getInstructionNodeFor(&I);
            addDataEdge(sourceNode, destNode);
        }
    }
    visitTerminatorInst(I);
}

void PDGBuilder::visitLoadInst(llvm::LoadInst& I)
{
    // TODO: output this for debug mode only
    llvm::dbgs() << "Load Inst: " << I << "\n";
    auto destNode = PDGNodeTy(new PDGLLVMInstructionNode(&I));
    m_currentFPDG->addNode(&I, destNode);
    connectToDefSite(&I, destNode);
}

void PDGBuilder::visitStoreInst(llvm::StoreInst& I)
{
    // TODO: output this for debug mode only
    llvm::dbgs() << "Store Inst: " << I << "\n";
    auto* valueOp = I.getValueOperand();
    auto sourceNode = getNodeFor(valueOp);
    if (!sourceNode) {
        return;
    }
    auto destNode = PDGNodeTy(new PDGLLVMInstructionNode(&I));
    addDataEdge(sourceNode, destNode);
    m_currentFPDG->addNode(&I, destNode);
}

void PDGBuilder::visitGetElementPtrInst(llvm::GetElementPtrInst& I)
{
    llvm::dbgs() << "GetElementPtr Inst: " << I << "\n";
    // TODO: see if needs special implementation
    visitInstruction(I);
}

void PDGBuilder::visitPhiNode(llvm::PHINode& I)
{
    llvm::dbgs() << "Phi Inst: " << I << "\n";
    // TODO: see if needs special implementation
    visitInstruction(I);
}

void PDGBuilder::visitMemSetInst(llvm::MemSetInst& I)
{
    llvm::dbgs() << "MemSet Inst: " << I << "\n";
    // TODO: see if needs special implementation
    visitInstruction(I);
}

void PDGBuilder::visitMemCpyInst(llvm::MemCpyInst& I)
{
    llvm::dbgs() << "MemCpy Inst: " << I << "\n";
    // TODO: see if needs special implementation
    visitInstruction(I);
}

void PDGBuilder::visitMemMoveInst(llvm::MemMoveInst &I)
{
    llvm::dbgs() << "MemMove Inst: " << I << "\n";
    // TODO: see if needs special implementation
    visitInstruction(I);
}

void PDGBuilder::visitMemTransferInst(llvm::MemTransferInst &I)
{
    llvm::dbgs() << "MemTransfer Inst: " << I << "\n";
    // TODO: see if needs special implementation
    visitInstruction(I);
}

void PDGBuilder::visitMemIntrinsic(llvm::MemIntrinsic &I)
{
    llvm::dbgs() << "MemInstrinsic Inst: " << I << "\n";
    // TODO: see if needs special implementation
    visitInstruction(I);
}

void PDGBuilder::visitCallInst(llvm::CallInst& I)
{
    // TODO: think about external calls
    llvm::CallSite callSite(&I);
    visitCallSite(callSite);
}

void PDGBuilder::visitInvokeInst(llvm::InvokeInst& I)
{
    llvm::CallSite callSite(&I);
    visitCallSite(callSite);
    visitTerminatorInst(I);
}

void PDGBuilder::visitTerminatorInst(llvm::TerminatorInst& I)
{
    auto sourceNode = getInstructionNodeFor(&I);
    for (unsigned i = 0; i < I.getNumSuccessors(); ++i) {
        auto* block = I.getSuccessor(i);
        auto destNode = getNodeFor(block);
        addControlEdge(sourceNode, destNode);
    }
}

void PDGBuilder::visitInstruction(llvm::Instruction& I)
{
    auto destNode = getInstructionNodeFor(&I);
    for (auto op_it = I.op_begin(); op_it != I.op_end(); ++op_it) {
        auto sourceNode = getNodeFor(op_it->get());
        addDataEdge(sourceNode, destNode);
    }
}

void PDGBuilder::visitCallSite(llvm::CallSite& callSite)
{
    auto destNode = getInstructionNodeFor(callSite.getInstruction());
    FunctionSet callees;
    if (!m_indCSResults->hasIndCSCallees(callSite)) {
        if (auto* calledF = callSite.getCalledFunction()) {
            callees.insert(calledF);
        }
    } else {
        callees = m_indCSResults->getIndCSCallees(callSite);
    }
    for (unsigned i = 0; i < callSite.getNumArgOperands(); ++i) {
        if (auto* val = llvm::dyn_cast<llvm::Value>(callSite.getArgOperand(i))) {
            auto sourceNode = getNodeFor(val);
            if (val->getType()->isPointerTy()) {
                llvm::dbgs() << *val << "\n";
                connectToDefSite(val, sourceNode);
            }
            auto actualArgNode = PDGNodeTy(new PDGLLVMActualArgumentNode(callSite, val));
            addDataEdge(sourceNode, actualArgNode);
            addDataEdge(actualArgNode, destNode);
            m_currentFPDG->addNode(actualArgNode);
            // connect actual args with formal args
            addActualArgumentNodeConnections(actualArgNode, i, callees);
        }
    }
}

void PDGBuilder::addDataEdge(PDGNodeTy source, PDGNodeTy dest)
{
    PDGNode::PDGEdgeType edge = PDGNode::PDGEdgeType(new PDGDataEdge(source, dest));
    source->addOutEdge(edge);
    dest->addInEdge(edge);
}

void PDGBuilder::addControlEdge(PDGNodeTy source, PDGNodeTy dest)
{
    PDGNode::PDGEdgeType edge = PDGNode::PDGEdgeType(new PDGControlEdge(source, dest));
    source->addOutEdge(edge);
    dest->addInEdge(edge);
}

PDGBuilder::PDGNodeTy PDGBuilder::getInstructionNodeFor(llvm::Instruction* instr)
{
    if (m_currentFPDG->hasNode(instr)) {
        return m_currentFPDG->getNode(instr);
    }
    m_currentFPDG->addNode(instr, PDGNodeTy(new PDGLLVMInstructionNode(instr)));
    return m_currentFPDG->getNode(instr);
}

PDGBuilder::PDGNodeTy PDGBuilder::getNodeFor(llvm::Value* value)
{
    if (m_currentFPDG->hasNode(value)) {
        return m_currentFPDG->getNode(value);
    }
    if (auto* global = llvm::dyn_cast<llvm::GlobalVariable>(value)) {
        if (!m_pdg->hasGlobalVariableNode(global)) {
            m_pdg->addGlobalVariableNode(global);
        }
        return m_pdg->getGlobalVariableNode(global);
    }
    if (auto* argument = llvm::dyn_cast<llvm::Argument>(value)) {
        assert(m_currentFPDG->hasFormalArgNode(argument));
        return m_currentFPDG->getFormalArgNode(argument);
    }

    if (auto* constant = llvm::dyn_cast<llvm::Constant>(value)) {
        m_currentFPDG->addNode(value, PDGNodeTy(new PDGLLVMConstantNode(constant)));
    } else if (auto* instr = llvm::dyn_cast<llvm::Instruction>(value)) {
        m_currentFPDG->addNode(value, PDGNodeTy(new PDGLLVMInstructionNode(instr)));
    } else {
        // do not assert here for now to keep track of possible values to be handled here
        llvm::dbgs() << "Unhandled value " << *value << "\n";
        return PDGNodeTy();
    }
    return m_currentFPDG->getNode(value);
}

PDGBuilder::PDGNodeTy PDGBuilder::getNodeFor(llvm::BasicBlock* block)
{
    if (!m_currentFPDG->hasNode(block)) {
        m_currentFPDG->addNode(block, PDGNodeTy(new PDGLLVMBasicBlockNode(block)));
    }
    return m_currentFPDG->getNode(block);
}

void PDGBuilder::connectToDefSite(llvm::Value* value, PDGNodeTy valueNode)
{
    PDGNodeTy sourceNode;
    auto* sourceInst = m_ptDefUse->getDefSite(value);
    if (!sourceInst || !m_currentFPDG->hasNode(sourceInst)) {
        if (sourceNode = m_ptDefUse->getDefSiteNode(value)) {
            if (sourceInst) {
                m_currentFPDG->addNode(sourceInst, sourceNode);
            } else {
                addPhiNodeConnections(sourceNode);
            }
        }
    } else {
        sourceNode = m_currentFPDG->getNode(sourceInst);
    }
    if (sourceNode) {
        addDataEdge(sourceNode, valueNode);
        return;
    }
    sourceInst = m_scalarDefUse->getDefSite(value);
    if (!sourceInst || !m_currentFPDG->hasNode(sourceInst)) {
        if (sourceNode = m_scalarDefUse->getDefSiteNode(value)) {
            if (sourceInst) {
                m_currentFPDG->addNode(sourceInst, sourceNode);
            } else {
                addPhiNodeConnections(sourceNode);
            }
        }
    } else {
        sourceNode = m_currentFPDG->getNode(sourceInst);
    }
    if (sourceNode) {
        addDataEdge(sourceNode, valueNode);
    }
}

void PDGBuilder::addActualArgumentNodeConnections(PDGNodeTy actualArgNode,
                                                  unsigned argIdx,
                                                  const FunctionSet& callees)
{
    for (auto& F : callees) {
        if (!m_pdg->hasFunctionPDG(F)) {
            buildFunctionDefinition(F);
        }
        FunctionPDGTy calleePDG = m_pdg->getFunctionPDG(F);
        // TODO: consider varargs
        llvm::Argument* formalArg = &*(F->arg_begin() + argIdx);
        auto formalArgNode = calleePDG->getFormalArgNode(formalArg);
        addDataEdge(actualArgNode, formalArgNode);
    }
}

void PDGBuilder::addPhiNodeConnections(PDGNodeTy node)
{
    PDGPhiNode* phiNode = llvm::dyn_cast<PDGPhiNode>(node.get());
    if (!phiNode) {
        return;
    }
    m_currentFPDG->addNode(node);
    for (unsigned i = 0; i < phiNode->getNumValues(); ++i) {
        llvm::Value* value = phiNode->getValue(i);
        auto destNode = getNodeFor(value);
        addDataEdge(destNode, node);
    }
}

} // namespace pdg
