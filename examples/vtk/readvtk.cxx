//
// Render two different VTK files side-by-side w/scaled color by attribute.
//
// Use VTK 6.3  (Is what I tested on.)
//
//      -Chad
//
//
#include <vtkSmartPointer.h>
#include <vtkDataSetMapper.h>
#include <vtkActor.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkRenderWindowInteractor.h>

#include <vtkUnstructuredGridReader.h>
#include <vtkUnstructuredGrid.h>
#include <vtkPointData.h>
#include <vtkLookupTable.h>
#include <vtkFloatArray.h>
#include <vtkCellData.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkIndent.h>

#include <vtkNamedColors.h>


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
    readerA->ReadAllScalarsOn();
    readerA->Update();

    //FILE B: read all the data from the file
    vtkSmartPointer<vtkUnstructuredGridReader> readerB =
        vtkSmartPointer<vtkUnstructuredGridReader>::New();
    readerB->SetFileName(fileB.c_str());
    readerB->Update();

    //COLOR A: Build a RGB color map automatically.
    vtkSmartPointer<vtkLookupTable> lutA =
        vtkSmartPointer<vtkLookupTable>::New();
    lutA->SetAlphaRange(0.3, 0.1);
    
    //COLOR B: Build a color map manually.
    vtkSmartPointer<vtkLookupTable> lutB =
        vtkSmartPointer<vtkLookupTable>::New();
    int colorCountB = 512;
    lutB->SetNumberOfTableValues(colorCountB);
    lutB->Build();
    double rgba[4] = {0.0, 0.0, 1.0, 1.0};
    lutB->SetTableValue(0, rgba);
    double colorStep = 1.0 * (1.0 / (double)colorCountB);
    int i;
    for (i = 1; i < colorCountB; i++) {
        rgba[0] = rgba[0] + colorStep;
        rgba[1] = 0.0;  //rgba[1] - colorStep; 
        rgba[2] = rgba[2] - colorStep;
        rgba[3] = 1.0; 
        lutB->SetTableValue(i, rgba);
    }

    //FILE A: Create a mapper and actor
    vtkSmartPointer<vtkDataSetMapper> mapperA =
        vtkSmartPointer<vtkDataSetMapper>::New();
    mapperA->SetInputConnection(readerA->GetOutputPort());
    //Find the min/max scalars to set the range...
    int              arrayIndex      = 5; //0 == max_backlog, 5 == loops
    vtkDataArray    *arrayData       = mapperA->GetInput()->GetCellData()->GetArray(arrayIndex);
    char            *arrayName       = arrayData->GetName();
    int              arrayTuples     = arrayData->GetNumberOfTuples();
    int              arrayComponents = arrayData->GetNumberOfComponents();

    double arrayMin = 0.0;
    double arrayMax = 0.0;
    double thisValue = 0.0;

    int tup, comp;
    for (tup = 0; tup < arrayTuples; tup++) {
        for (comp = 0; comp < arrayComponents; comp++) {
            thisValue = arrayData->GetComponent(tup, comp);
            arrayMin = std::min(arrayMin, thisValue);
            arrayMax = std::max(arrayMax, thisValue);
        }
    }

    std::cout << "array[" << arrayName << "]: min=" << arrayMin << "  max=" << arrayMax << std::endl;

    //
    //
    mapperA->SetScalarRange(arrayMin, arrayMax);
    mapperA->SetScalarModeToUseCellData();
    mapperA->GetInput()->GetCellData()->SetActiveScalars("rank");

    // mapperA->SelectColorArray("max_backlog");          //<-- DEPRECATED
    mapperA->GetInput()->PrintSelf(cout, vtkIndent()); //<-- DEBUG

    mapperA->SetLookupTable(lutA);
    vtkSmartPointer<vtkActor> actorA =
        vtkSmartPointer<vtkActor>::New();
    actorA->SetMapper(mapperA);

    //FILE B: Create a mapper and actor
    vtkSmartPointer<vtkDataSetMapper> mapperB =
        vtkSmartPointer<vtkDataSetMapper>::New();
    mapperB->SetInputConnection(readerB->GetOutputPort());
    mapperB->SetScalarRange(0, colorCountB - 1);
    mapperB->SetLookupTable(lutB);
    //mapperB->SelectColorArray();
    vtkSmartPointer<vtkActor> actorB =
        vtkSmartPointer<vtkActor>::New();
    actorB->SetMapper(mapperB);


    //Create a renderer, render window, and interactor
    vtkSmartPointer<vtkRenderer> renderer =
        vtkSmartPointer<vtkRenderer>::New();
    vtkSmartPointer<vtkRenderWindow> renderWindow =
        vtkSmartPointer<vtkRenderWindow>::New();
    renderWindow->SetSize(1200, 600);
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
    leftRenderer->SetBackground(1.0, 1.0, 1.0);  //(0.6, 0.5, 0.4);

    vtkSmartPointer<vtkRenderer> rightRenderer =
        vtkSmartPointer<vtkRenderer>::New();
    renderWindow->AddRenderer(rightRenderer);
    rightRenderer->SetViewport(rightViewport);
    rightRenderer->SetBackground(1.0, 1.0, 1.0);  //(0.4, 0.5, 0.6);

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
