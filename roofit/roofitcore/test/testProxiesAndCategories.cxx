// Tests for the RooTemplateProxy and RooCategory, and if they work together
// Author: Stephan Hageboeck, CERN  01/2019

#include "RooTemplateProxy.h"
#include "RooCategoryProxy.h"

#include "RooRealVar.h"
#include "RooAbsPdf.h"
#include "RooCategory.h"
#include "RooDataSet.h"
#include "RooGenericPdf.h"
#include "RooWorkspace.h"
#include "TFile.h"
#include "TMemFile.h"

#include "gtest/gtest.h"


TEST(RooCategory, CategoryDefineMultiState) {
  RooCategory myCat("myCat", "A category", { {"0Lep", 0}, {"1Lep", 1}, {"2Lep", 2}, {"3Lep", 3} });

  std::vector<std::string> targets{"0Lep", "1Lep", "2Lep", "3Lep"};
  int i = 0;
  for (const auto& nameAndIdx : myCat) {
    EXPECT_EQ(nameAndIdx.second, i);
    EXPECT_EQ(nameAndIdx.first, targets[i]);
    i++;
  }
  EXPECT_EQ(myCat.lookupName(1), "1Lep");
  EXPECT_EQ(myCat.lookupIndex("2Lep"), 2);
}


TEST(RooCategory, WriteAndReadBack) {
  RooCategory myCat("myCat", "A category", {
      {"0Lep", 0}, {"1Lep", 1}, {"2Lep", 2}, {"3Lep", 3} });
  myCat.setIndex(2);

  TMemFile file("memfile", "RECREATE");
  file.WriteObject(&myCat, "myCat");

  RooCategory* readCat = nullptr;
  file.GetObject("myCat", readCat);
  ASSERT_NE(readCat, nullptr);
  EXPECT_EQ(readCat->getIndex(), myCat.getIndex());
  EXPECT_STREQ(readCat->getLabel(), myCat.getLabel());
  EXPECT_EQ(readCat->size(), 4u);

  auto readIt = std::next(readCat->begin());
  auto origIt = std::next(myCat.begin());
  EXPECT_EQ(readIt->first, origIt->first);
  EXPECT_EQ(readIt->second, origIt->second);
}


TEST(RooCategory, BracketOperator) {
  RooCategory myCat;
  myCat["0Lep"] = 0;
  myCat["1Lep"];
  myCat["Negative"] = -1;
  myCat["2Lep"];

  std::map<int, std::string> targets{{-1,"Negative"}, {0,"0Lep"}, {1,"1Lep"}, {2,"2Lep"}};
  for (const auto& nameAndIndex : myCat) {
    ASSERT_NE(targets.find(nameAndIndex.second), targets.end());
    EXPECT_EQ(nameAndIndex.first, targets[nameAndIndex.second]);
  }
}


struct DummyClass : public RooAbsPdf {
    DummyClass(RooAbsCategory& theCat, RooRealVar& theVar, RooAbsPdf* thePdf = nullptr) :
      cat("catProxy", "Stores categories", this, theCat),
      var("varProxy", "Stores variables", this, theVar),
      pdf("pdfProxy", "Stores pdfs", this) {
      if (thePdf) {
        pdf.setArg(*thePdf);
      }
    }

    double evaluate() const {
      return 1.;
    }

    void clear() {
      clearValueAndShapeDirty();
    }

    TObject* clone(const char*) const {
      return new TObject();
    }

    RooCategoryProxy cat;
    RooTemplateProxy<RooRealVar> var;
    RooTemplateProxy<RooAbsPdf>  pdf;
};


TEST(RooTemplateProxy, CategoryProxy) {
  RooCategory myCat("myCat", "A category");
  myCat.defineType("A", 1);
  myCat.defineType("B", 2);
  myCat.defineType("NotA", -1);
  std::string longStr(500, '*');
  myCat.defineType(longStr, 500);

  RooRealVar x("x", "x", -10, 10);

  DummyClass dummy(myCat, x);
  dummy.clear();

  dummy.cat = 2;
  EXPECT_TRUE(dummy.isValueDirty());
  dummy.clear();
  EXPECT_TRUE(dummy.cat == 2);
  EXPECT_STREQ(dummy.cat.label(), "B");

  dummy.cat = longStr.c_str();
  EXPECT_TRUE(dummy.isValueDirty());
  dummy.clear();
  EXPECT_TRUE(dummy.cat == 500);
  EXPECT_STREQ(dummy.cat.label(), longStr.c_str());

  dummy.cat = std::string("NotA");
  EXPECT_TRUE(dummy.isValueDirty());
  dummy.clear();
  EXPECT_TRUE(dummy.cat == -1);
  EXPECT_STREQ(dummy.cat.label(), "NotA");

  dummy.var = 2.;
  EXPECT_TRUE(dummy.isValueDirty());
  dummy.clear();
  EXPECT_TRUE(dummy.var == 2.);
}


TEST(RooTemplateProxy, DISABLED_CategoryProxyBatchAccess) {
  RooCategory myCat("myCat", "A category");
  myCat.defineType("A", 1);
  myCat.defineType("B", 2);
  myCat.defineType("NotA", -1);
  std::string longStr(500, '*');
  myCat.defineType(longStr, 500);

  RooRealVar x("x", "x", -10, 10);

  DummyClass dummy(myCat, x);

  RooDataSet data("data", "data", RooArgSet(x, myCat));
  auto& xData = dynamic_cast<RooRealVar&>((*data.get())["x"]);
  auto& catData = dynamic_cast<RooAbsCategoryLValue&>((*data.get())["myCat"]);
  for (unsigned int i=0; i < 9; ++i) {
    xData = i;
    catData = i%2 + 1;
    data.fill();
  }
  xData = 9;
  catData = -1;
  data.fill();

  data.attachBuffers(*dummy.getVariables());

  ASSERT_THROW(dummy.cat->getValBatch(0, 10), std::logic_error);
  return; // Have to stop here for now until above is implemented.
  auto theBatch = dummy.cat.getValBatch(0, 10);
  ASSERT_FALSE(theBatch.empty());
  EXPECT_EQ(theBatch.size(), 10ul);
  EXPECT_EQ(theBatch[0], 1);
  EXPECT_EQ(theBatch[1], 2);
  EXPECT_EQ(theBatch[8], 1);
  EXPECT_EQ(theBatch[9], -1);
}


TEST(RooTemplateProxy, RealProxyBatchAccess) {
  RooCategory myCat("myCat", "A category");
  RooRealVar x("x", "x", -10, 10);
  DummyClass dummy(myCat, x);

  RooDataSet data("data", "data", x);
  RooRealVar& xData = dynamic_cast<RooRealVar&>((*data.get())["x"]);
  for (unsigned int i=0; i < 10; ++i) {
    xData = i;
    data.fill();
  }

  auto dummyObservables = dummy.getObservables(data);
  data.attachBuffers(*dummyObservables);

  auto theBatch = dummy.var->getValBatch(0, 10);
  ASSERT_FALSE(theBatch.empty());
  EXPECT_EQ(theBatch.size(), 10ul);
  EXPECT_EQ(theBatch[2], 2.);
  EXPECT_EQ(theBatch[9], 9.);

  auto largerBatch = dummy.var.getValBatch(0, 100);
  EXPECT_EQ(largerBatch.size(), 10ul);

  dummy.var = 1.337;
  EXPECT_EQ(dummy.var * 1., 1.337);
}


TEST(RooTemplateProxy, PdfProxyBatchAccess) {
  RooCategory myCat("myCat", "A category");
  RooRealVar x("x", "x", -10, 10);
  RooGenericPdf generic("generic", "generic", "1.+x", x);
  DummyClass dummy(myCat, x, &generic);

  RooDataSet data("data", "data", x);
  RooRealVar& xData = dynamic_cast<RooRealVar&>((*data.get())["x"]);
  for (unsigned int i=0; i < 10; ++i) {
    xData = i;
    data.fill();
  }
  data.attachBuffers(*dummy.getVariables());

  auto theBatch = dummy.pdf->getValBatch(0, 10);
  ASSERT_FALSE(theBatch.empty());
  EXPECT_EQ(theBatch.size(), 10ul);
  EXPECT_EQ(theBatch[2], 3.);
  EXPECT_EQ(theBatch[9], 10.);
}