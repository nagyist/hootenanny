/*
 * This file is part of Hootenanny.
 *
 * Hootenanny is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * --------------------------------------------------------------------
 *
 * The following copyright notices are generated automatically. If you
 * have a new notice to add, please use the format:
 * " * @copyright Copyright ..."
 * This will properly maintain the copyright information. Maxar
 * copyrights will be updated automatically.
 *
 * @copyright Copyright (C) 2022 Maxar (http://www.maxar.com/)
 */

// Hoot
#include <hoot/core/TestUtils.h>
#include <hoot/core/io/MultiFileWriter.h>

namespace hoot
{

class MultiFileWriterTest : public HootTestFixture
{
  CPPUNIT_TEST_SUITE(MultiFileWriterTest);
  CPPUNIT_TEST(runBufferTest);
  CPPUNIT_TEST(runSingleFileTest);
  CPPUNIT_TEST(runGeometryTest);
  CPPUNIT_TEST(runFCodeTest);
  CPPUNIT_TEST_SUITE_END();

public:

  MultiFileWriterTest()
    : HootTestFixture("test-files/io/MultiFileWriter/",
                      "test-output/io/MultiFileWriter/")
  {
  }

  void runBufferTest()
  {
    //  Indices don't matter for the buffer test
    std::vector<QString> indices({""});
    //  Create the single buffer object
    MultiFileWriter writer(MultiFileWriter::MultiFileWriterType::SingleBuffer);
    //  Open
    writer.open();
    //  Write all the testing data to the buffer
    writeData(writer, indices);
    QString value = writer.getBuffer();
    //  Close the writer
    writer.close();
    //  Test the content
    HOOT_STR_EQUALS(FileUtils::readFully(_inputPath + "SingleFileTest.json"), value);
  }

  void runSingleFileTest()
  {
    QString base = "SingleFileTest";
    QString extension = ".json";
    //  Indices don't matter for the single file test
    std::vector<QString> indices({""});
    //  Run the single file object test
    runTest(base, extension, indices, MultiFileWriter::SingleFile);
  }

  void runGeometryTest()
  {
    QString base = "GeometryTest";
    QString extension = ".json";
    //  Split the output into three different files
    std::vector<QString> indices({MultiFileWriter::POINTS, MultiFileWriter::LINES, MultiFileWriter::POLYGONS});
    //  Run multi-file geometry test
    runTest(base, extension, indices, MultiFileWriter::MultiGeom);
  }

  void runFCodeTest()
  {
    QString base = "FCodeTest";
    QString extension = ".json";
    //  Split the output into 12 files
    std::vector<QString> indices({"AAL200", "ABA030", "ABA040", "ABH080", "ABH090", "ABH140", "ABH160", "ADA010", "AEA010", "AEB010", "AEB020", "AEC030"});
    //  Run the multi-file F-Code test
    runTest(base, extension, indices, MultiFileWriter::MultiFCode);
  }

  void runTest(const QString& base, const QString& ext, const std::vector<QString>& indices, MultiFileWriter::MultiFileWriterType type)
  {
    //  Create the multi-file object
    MultiFileWriter writer(type);
    //  Open the file
    writer.open(QString("%1%2%3").arg(_outputPath, base, ext));
    //  Write all the testing data to the object
    writeData(writer, indices);
    //  Close the writer
    writer.close();
    //  Test the output
    checkFiles(base, ext, indices);
  }

  void writeData(MultiFileWriter& writer, const std::vector<QString>& indices)
  {
    int total_elements = 24;
    int elements_per_file = total_elements / static_cast<int>(indices.size());
    int index = 0;
    //  Write a bunch of things to the header
    writer.writeHeader("{");
    writer.writeHeader("\"file\": \"This is the header\",\n");
    writer.writeHeader("\"contents\": [\n");
    //  Iterate a total number of elements
    for (int i = 0; i < total_elements; ++i)
    {
      //  Update the index
      index = i / elements_per_file;
      writer.setCurrentFileIndex(indices[index]);
      //  Write the content
      writer.write("{ \"element-id\": \"");
      writer.write(QString::number(i));
      writer.write("\", \"element-type\": \"");
      writer.write(indices[index]);
      writer.write("\" }");
      if (i % elements_per_file != elements_per_file - 1)
        writer.write(",");
      writer.write("\n");
    }
    //  Write the footer
    writer.writeFooter("]");
    writer.writeFooter("}\n");
  }

  void checkFiles(const QString& base, const QString& ext, const std::vector<QString>& indices)
  {
    //  Test the output
    for (const auto& index : indices)
    {
      QString dash = (index.isEmpty() ? "" : "-");
      HOOT_FILE_EQUALS(QString("%1%2%3%4%5").arg(_inputPath, base, dash, index, ext),
                       QString("%1%2%3%4%5").arg(_outputPath, base, dash, index, ext));
    }
  }

};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(MultiFileWriterTest, "quick");

}