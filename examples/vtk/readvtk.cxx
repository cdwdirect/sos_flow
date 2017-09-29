// note: use vtk6 -chad
//
// to color the cells, see this:
//  https://lorensen.github.io/VTKExamples/site/Cxx/PolyData/ColorCells/
//
//
#include <vtkUnstructuredGridReader.h>
#include <vtkSmartPointer.h>
#include <vtkDataSetMapper.h>
#include <vtkActor.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkRenderWindowInteractor.h>

int main ( int argc, char *argv[] )
{
  //parse command line arguments
  if(argc != 3)
  {
    std::cerr << "Usage: " << argv[0]
              << " FileA(.vtk) FileB(.vtk)" << std::endl;
    return EXIT_FAILURE;
  }

  std::string fileA = argv[1];
  std::string fileB = argv[2];

  //FILE A: read all the data from the file
  vtkSmartPointer<vtkUnstructuredGridReader> readerA =
    vtkSmartPointer<vtkUnstructuredGridReader>::New();
  readerA->SetFileName(fileA.c_str());
  readerA->Update();
  //FILE A: Create a mapper and actor
  vtkSmartPointer<vtkDataSetMapper> mapperA =
    vtkSmartPointer<vtkDataSetMapper>::New();
  mapperA->SetInputConnection(readerA->GetOutputPort());
  vtkSmartPointer<vtkActor> actorA =
    vtkSmartPointer<vtkActor>::New();
  actorA->SetMapper(mapperA);

  //FILE B: read all the data from the file
  vtkSmartPointer<vtkUnstructuredGridReader> readerB =
    vtkSmartPointer<vtkUnstructuredGridReader>::New();
  readerB->SetFileName(fileB.c_str());
  readerB->Update();
  //FILE B: Create a mapper and actor
  vtkSmartPointer<vtkDataSetMapper> mapperB =
    vtkSmartPointer<vtkDataSetMapper>::New();
  mapperB->SetInputConnection(readerB->GetOutputPort());
  vtkSmartPointer<vtkActor> actorB =
    vtkSmartPointer<vtkActor>::New();
  actorB->SetMapper(mapperB);


  //Create a renderer, render window, and interactor
  vtkSmartPointer<vtkRenderer> renderer =
    vtkSmartPointer<vtkRenderer>::New();
  vtkSmartPointer<vtkRenderWindow> renderWindow =
    vtkSmartPointer<vtkRenderWindow>::New();
  renderWindow->SetSize(600, 300);
    // Add one interactor: 
  vtkSmartPointer<vtkRenderWindowInteractor> renderWindowInteractor =
    vtkSmartPointer<vtkRenderWindowInteractor>::New();
  renderWindowInteractor->SetRenderWindow(renderWindow);
  
  //Define viewport ranges:
  double leftViewport[4]  = {0.0, 0.0, 0.5, 1.0};
  double rightViewport[4] = {0.5, 0.0, 1.0, 1.0};
  
    //Set up both renderers:
    vtkSmartPointer<vtkRenderer> leftRenderer =
        vtkSmartPointer<vtkRenderer>::New();
    renderWindow->AddRenderer(leftRenderer);
    leftRenderer->SetViewport(leftViewport);
    leftRenderer->SetBackground(0.6, 0.5, 0.4);

    vtkSmartPointer<vtkRenderer> rightRenderer =
        vtkSmartPointer<vtkRenderer>::New();
    renderWindow->AddRenderer(rightRenderer);
    rightRenderer->SetViewport(rightViewport);
    rightRenderer->SetBackground(0.4, 0.5, 0.6);

  //Add the actor to the scene
  leftRenderer->AddActor(actorA);
  rightRenderer->AddActor(actorB);

  leftRenderer->ResetCamera();
  rightRenderer->ResetCamera();

  //Render and interact
  renderWindow->Render();
  renderWindowInteractor->Start();

  return EXIT_SUCCESS;
}
