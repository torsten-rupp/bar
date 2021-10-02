#!groovy

//TODO: store docker image in registry?
//dockerImageName="dockerregistry.macio.de:5000/macio_backup_archiver"
dockerImageName='macio_backup_archiver'

// build
def build()
{
  try
  {
    stage("Docker image")
    {
      checkout scm
      sh "(cd docker; docker build -t ${dockerImageName} .)"
    }


    stage("Build")
    {
      docker.image(dockerImageName).inside
      {
        checkout scm
        sh "./download-third-party-packages.sh"
        sh "./configure"
//TODO
//        sh "make -j`nproc --ignore=1`"
        sh "make"
        sh "make dist"
      }
    }

    stage("Packages")
    {
      docker.image(dockerImageName).inside
      {
//TODO: how to run a docker from inside a docker?
//        sh "make update_docker"
//        sh "make rpm"
//        sh "make deb"
        sh "make gui"
      }
    }

    stage("Tests")
    {
//TODO
//      docker.image(dockerImageName).inside
//      {
//        sh "make test_min-debug O='--verbose=1'"
//      }

      script
      {
        switch (params.BINARY_TYPE)
        {
          case "DEBUG":
            binaryExtension="-debug";
            break;
          case "VALGRIND":
            binaryExtension="-valgrind";
            break;
          case "GCOV":
            binaryExtension="-gcov";
            break;
          case "GPROF":
            binaryExtension="-gprof";
            break;
          default:
            binaryExtension="";
            break;
        }

        switch (params.TEST_TYPE)
        {
          case "SMOKE":
            docker.image(dockerImageName).inside
            {
              sh "echo XXXXmake test_smoke-"+binaryExtension+" O='--verbose=1'"
            }
            break;
          case "FULL":
            docker.image(dockerImageName).inside
            {
              sh "make test1-debug O='--verbose=1'"
              sh "make test2-debug O='--verbose=1'"
              sh "make test3-debug O='--verbose=1'"
              sh "make test4-debug O='--verbose=1'"
              sh "make test5-debug O='--verbose=1'"
              sh "make test6-debug O='--verbose=1'"
              sh "make test7-debug O='--verbose=1'"
            }
            break;
        }
        if (params.FULL_TEST)
        {
          docker.image(dockerImageName).inside
          {
            sh "make test1-debug O='--verbose=1'"
            sh "make test2-debug O='--verbose=1'"
            sh "make test3-debug O='--verbose=1'"
            sh "make test4-debug O='--verbose=1'"
            sh "make test5-debug O='--verbose=1'"
            sh "make test6-debug O='--verbose=1'"
            sh "make test7-debug O='--verbose=1'"
//            sh "make test1-valgrind O='--verbose=1'"
//            sh "make test2-valgrind O='--verbose=1'"
          }
        }
      }
    }
  }
  finally
  {
    archiveArtifacts artifacts: 'backup-archiver*.tar.bz2, *.rpm, *.deb, *.zip', allowEmptyArchive: 'true'
    cleanWs notFailBuild: true
  }
}

// main
properties([gitLabConnection('GitLab-DMZ')])
try
{
  node('docker')
  {
    build()
  }
}
catch (error)
{
  echo "ERROR: ${error.message}"
  currentBuild.result = 'FAILED'
}
